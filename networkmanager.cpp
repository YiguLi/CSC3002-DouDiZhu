#include "networkmanager.h"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QJsonArray>
#include <QDebug>

// ==================== 辅助函数 ====================

QString messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::PlayerJoin: return "PlayerJoin";
        case MessageType::PlayerLeave: return "PlayerLeave";
        case MessageType::PlayerList: return "PlayerList";
        case MessageType::GameStart: return "GameStart";
        case MessageType::DealCards: return "DealCards";
        case MessageType::CallLandlord: return "CallLandlord";
        case MessageType::LandlordConfirm: return "LandlordConfirm";
        case MessageType::SendLandlordCards: return "SendLandlordCards";
        case MessageType::PlayerDiscard: return "PlayerDiscard";
        case MessageType::PlayerPass: return "PlayerPass";
        case MessageType::TurnChange: return "TurnChange";
        case MessageType::GameOver: return "GameOver";
        case MessageType::Error: return "Error";
        case MessageType::Ping: return "Ping";
        case MessageType::Pong: return "Pong";
        default: return "Unknown";
    }
}

MessageType stringToMessageType(const QString& str) {
    if (str == "PlayerJoin") return MessageType::PlayerJoin;
    if (str == "PlayerLeave") return MessageType::PlayerLeave;
    if (str == "PlayerList") return MessageType::PlayerList;
    if (str == "GameStart") return MessageType::GameStart;
    if (str == "DealCards") return MessageType::DealCards;
    if (str == "CallLandlord") return MessageType::CallLandlord;
    if (str == "LandlordConfirm") return MessageType::LandlordConfirm;
    if (str == "SendLandlordCards") return MessageType::SendLandlordCards;
    if (str == "PlayerDiscard") return MessageType::PlayerDiscard;
    if (str == "PlayerPass") return MessageType::PlayerPass;
    if (str == "TurnChange") return MessageType::TurnChange;
    if (str == "GameOver") return MessageType::GameOver;
    if (str == "Error") return MessageType::Error;
    if (str == "Ping") return MessageType::Ping;
    if (str == "Pong") return MessageType::Pong;
    return MessageType::Error;
}

// ==================== GameServer ====================

GameServer::GameServer(QObject *parent)
    : QObject(parent)
    , m_server(nullptr)
    , m_nextPlayerId(1) // 服务器自己是玩家0
{
}

GameServer::~GameServer()
{
    stopServer();
}

bool GameServer::startServer(quint16 port)
{
    if (m_server) {
        stopServer();
    }
    
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &GameServer::onNewConnection);
    
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit serverError(QString("无法启动服务器: %1").arg(m_server->errorString()));
        delete m_server;
        m_server = nullptr;
        return false;
    }
    
    qDebug() << "服务器已启动，监听端口:" << port;
    return true;
}

void GameServer::stopServer()
{
    if (m_server) {
        // 断开所有客户端
        for (auto socket : m_clients.values()) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
        m_clients.clear();
        m_socketToId.clear();
        m_playerNames.clear();
        
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

QString GameServer::getLocalIP() const
{
    // 获取本机局域网IP地址
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol 
            && !address.isLoopback()
            && address.toString().startsWith("192.168.")) {
            return address.toString();
        }
    }
    
    // 如果没找到192.168开头的，返回任意非回环IPv4地址
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol 
            && !address.isLoopback()) {
            return address.toString();
        }
    }
    
    return "127.0.0.1";
}

void GameServer::onNewConnection()
{
    QTcpSocket* socket = m_server->nextPendingConnection();
    
    if (m_clients.size() >= 2) { // 最多2个客户端（加上服务器共3人）
        socket->write("FULL");
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }
    
    int playerId = m_nextPlayerId++;
    m_clients[playerId] = socket;
    m_socketToId[socket] = playerId;
    
    connect(socket, &QTcpSocket::readyRead, this, &GameServer::onClientReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &GameServer::onClientDisconnected);
    
    qDebug() << "新玩家连接，ID:" << playerId;
}

void GameServer::onClientReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    int playerId = m_socketToId.value(socket, -1);
    if (playerId == -1) return;
    
    QByteArray data = socket->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        qDebug() << "收到无效的JSON数据";
        return;
    }
    
    processMessage(playerId, doc.object());
}

void GameServer::onClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    int playerId = m_socketToId.value(socket, -1);
    if (playerId != -1) {
        m_clients.remove(playerId);
        m_socketToId.remove(socket);
        QString playerName = m_playerNames.take(playerId);
        
        qDebug() << "玩家断开连接，ID:" << playerId << "名称:" << playerName;
        emit playerLeft(playerId);
        
        // 通知其他客户端
        QJsonObject data;
        data["playerId"] = playerId;
        data["playerName"] = playerName;
        broadcastMessage(createMessage(MessageType::PlayerLeave, data));
    }
    
    socket->deleteLater();
}

void GameServer::processMessage(int playerId, const QJsonObject& message)
{
    QString typeStr = message["type"].toString();
    MessageType type = stringToMessageType(typeStr);
    QJsonObject data = message["data"].toObject();
    
    // 处理玩家加入消息
    if (type == MessageType::PlayerJoin) {
        QString playerName = data["playerName"].toString();
        m_playerNames[playerId] = playerName;
        
        qDebug() << "玩家加入:" << playerName << "ID:" << playerId;
        emit playerJoined(playerId, playerName);
        
        // 发送玩家ID给客户端
        QJsonObject response;
        response["playerId"] = playerId;
        sendToClient(playerId, createMessage(MessageType::PlayerJoin, response));
        
        // 广播玩家列表更新
        QJsonObject listData;
        QJsonArray players;
        for (auto it = m_playerNames.constBegin(); it != m_playerNames.constEnd(); ++it) {
            QJsonObject player;
            player["playerId"] = it.key();
            player["playerName"] = it.value();
            players.append(player);
        }
        listData["players"] = players;
        broadcastMessage(createMessage(MessageType::PlayerList, listData));
    } else {
        // 其他消息转发给应用层处理
        emit messageReceived(playerId, type, data);
    }
}

void GameServer::broadcastMessage(const QJsonObject& message)
{
    QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);
    data.append('\n'); // 添加换行符作为消息分隔符
    
    for (QTcpSocket* socket : m_clients.values()) {
        if (socket && socket->state() == QTcpSocket::ConnectedState) {
            socket->write(data);
            socket->flush();
        }
    }
}

void GameServer::sendToClient(int playerId, const QJsonObject& message)
{
    if (!m_clients.contains(playerId)) return;
    
    QTcpSocket* socket = m_clients[playerId];
    if (socket && socket->state() == QTcpSocket::ConnectedState) {
        QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);
        data.append('\n');
        socket->write(data);
        socket->flush();
    }
}

QJsonObject GameServer::createMessage(MessageType type, const QJsonObject& data)
{
    QJsonObject message;
    message["type"] = messageTypeToString(type);
    message["data"] = data;
    return message;
}

QStringList GameServer::getPlayerNames() const
{
    QStringList names;
    for (const QString& name : m_playerNames.values()) {
        names.append(name);
    }
    return names;
}

// ==================== GameClient ====================

GameClient::GameClient(QObject *parent)
    : QObject(parent)
    , m_socket(nullptr)
    , m_myPlayerId(-1)
{
}

GameClient::~GameClient()
{
    disconnectFromServer();
}

void GameClient::connectToServer(const QString& ip, quint16 port)
{
    if (m_socket) {
        disconnectFromServer();
    }
    
    m_socket = new QTcpSocket(this);
    
    connect(m_socket, &QTcpSocket::connected, this, &GameClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &GameClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &GameClient::onReadyRead);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &GameClient::onError);
    
    qDebug() << "正在连接到服务器:" << ip << ":" << port;
    m_socket->connectToHost(ip, port);
}

void GameClient::disconnectFromServer()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_myPlayerId = -1;
}

void GameClient::onConnected()
{
    qDebug() << "已连接到服务器";
    emit connected();
    
    // 发送加入消息
    QJsonObject data;
    data["playerName"] = m_playerName.isEmpty() ? "玩家" : m_playerName;
    sendMessage(createMessage(MessageType::PlayerJoin, data));
}

void GameClient::onDisconnected()
{
    qDebug() << "已断开与服务器的连接";
    emit disconnected();
}

void GameClient::onReadyRead()
{
    QByteArray data = m_socket->readAll();
    
    // 检查是否是房间已满的响应
    if (data == "FULL") {
        emit connectionError("房间已满");
        disconnectFromServer();
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qDebug() << "收到无效的JSON数据";
        return;
    }
    
    processMessage(doc.object());
}

void GameClient::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    QString errorStr = m_socket->errorString();
    qDebug() << "Socket错误:" << errorStr;
    emit connectionError(errorStr);
}

void GameClient::processMessage(const QJsonObject& message)
{
    QString typeStr = message["type"].toString();
    MessageType type = stringToMessageType(typeStr);
    QJsonObject data = message["data"].toObject();
    
    // 处理玩家加入响应（获取自己的ID）
    if (type == MessageType::PlayerJoin) {
        m_myPlayerId = data["playerId"].toInt();
        qDebug() << "收到玩家ID:" << m_myPlayerId;
    }
    
    emit messageReceived(type, data);
}

void GameClient::sendMessage(const QJsonObject& message)
{
    if (!m_socket || m_socket->state() != QTcpSocket::ConnectedState) {
        qDebug() << "无法发送消息：未连接到服务器";
        return;
    }
    
    QByteArray data = QJsonDocument(message).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_socket->write(data);
    m_socket->flush();
}

QJsonObject GameClient::createMessage(MessageType type, const QJsonObject& data)
{
    QJsonObject message;
    message["type"] = messageTypeToString(type);
    message["data"] = data;
    return message;
}
