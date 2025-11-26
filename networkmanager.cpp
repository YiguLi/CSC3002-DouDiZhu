#include "networkmanager.h"
#include "game.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostAddress>
#include <QDateTime>
#include <QDebug>

NetworkManager::NetworkManager(QObject* parent)
    : QObject(parent), game(nullptr), server(nullptr), clientSocket(nullptr),
      isHost(false), isConnected(false), gameStarted(false),
      localPlayerId(-1), nextPlayerId(1), useAI(false) {
}

NetworkManager::~NetworkManager() {
    LeaveRoom();
}

bool NetworkManager::CreateRoom(int port) {
    if (isConnected) {
        emit errorOccurred("已经在房间中");
        return false;
    }
    
    // 创建TCP服务器
    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, &NetworkManager::OnNewConnection);
    
    if (!server->listen(QHostAddress::Any, port)) {
        emit errorOccurred("无法创建房间: " + server->errorString());
        delete server;
        server = nullptr;
        return false;
    }
    
    isHost = true;
    isConnected = true;
    localPlayerId = 0;  // 房主ID为0
    nextPlayerId = 1;   // 下一个加入的玩家ID从1开始
    
    qDebug() << "[NetworkManager] 房间创建成功，端口:" << port;
    emit roomCreated(port);
    return true;
}

bool NetworkManager::JoinRoom(const QString& hostAddress, int port) {
    if (isConnected) {
        emit errorOccurred("已经在房间中");
        return false;
    }
    
    // 创建TCP客户端
    clientSocket = new QTcpSocket(this);
    connect(clientSocket, &QTcpSocket::connected, this, &NetworkManager::OnConnected);
    connect(clientSocket, &QTcpSocket::disconnected, this, &NetworkManager::OnDisconnected);
    connect(clientSocket, &QTcpSocket::readyRead, this, &NetworkManager::OnReadyRead);
    connect(clientSocket, &QTcpSocket::errorOccurred, this, &NetworkManager::OnSocketError);
    
    clientSocket->connectToHost(hostAddress, port);
    
    // 等待连接（非阻塞方式会在OnConnected中处理）
    qDebug() << "[NetworkManager] 正在连接到房主:" << hostAddress << ":" << port;
    return true;
}

void NetworkManager::LeaveRoom() {
    if (!isConnected) return;
    
    // 发送离开房间消息
    QJsonObject msg = CreateMessage(MSG_LEAVE_ROOM);
    if (isHost) {
        BroadcastMessage(msg);
    } else if (clientSocket) {
        SendMessage(clientSocket, msg);
    }
    
    // 清理连接
    if (server) {
        server->close();
        delete server;
        server = nullptr;
    }
    
    if (clientSocket) {
        clientSocket->disconnectFromHost();
        clientSocket->deleteLater();
        clientSocket = nullptr;
    }
    
    // 清理所有连接
    for (auto socket : connections.keys()) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    connections.clear();
    playerSockets.clear();
    
    isHost = false;
    isConnected = false;
    gameStarted = false;
    localPlayerId = -1;
    
    qDebug() << "[NetworkManager] 已离开房间";
}

void NetworkManager::SetPlayerReady(bool ready) {
    QJsonObject msg = CreateMessage(MSG_PLAYER_READY);
    msg["playerId"] = localPlayerId;
    msg["ready"] = ready;
    
    if (isHost) {
        // 房主的准备状态也需要广播
        BroadcastMessage(msg);
        emit playerReadyChanged(localPlayerId, ready);
    } else if (clientSocket) {
        SendMessage(clientSocket, msg);
    }
}

void NetworkManager::StartGame() {
    if (!isHost) {
        emit errorOccurred("只有房主可以开始游戏");
        return;
    }
    
    // 检查是否所有玩家都准备好了
    int playerCount = connections.size() + 1;  // +1 for host
    
    // 确定是否使用AI
    if (playerCount == 2) {
        useAI = true;  // 二人联机，需要AI
    } else if (playerCount == 3) {
        useAI = false; // 三人联机，不需要AI
    } else {
        emit errorOccurred("玩家数量不正确（需要2或3人）");
        return;
    }
    
    QJsonObject msg = CreateMessage(MSG_GAME_START);
    msg["useAI"] = useAI;
    msg["aiPlayerId"] = 2;  // AI的ID固定为2
    
    BroadcastMessage(msg);
    gameStarted = true;
    
    qDebug() << "[NetworkManager] 游戏开始，使用AI:" << useAI;
    // 房主也需要发出gameStartRequested信号来触发游戏界面
    emit gameStartRequested();
}

void NetworkManager::SendCallLandlord(int playerId, int score) {
    QJsonObject msg = CreateMessage(MSG_CALL_LANDLORD);
    msg["playerId"] = playerId;
    msg["score"] = score;
    
    if (isHost) {
        BroadcastMessage(msg);
    } else if (clientSocket) {
        SendMessage(clientSocket, msg);
    }
}

void NetworkManager::SendDiscardCards(int playerId, const std::vector<int>& cards) {
    QJsonObject msg = CreateMessage(MSG_DISCARD_CARDS);
    msg["playerId"] = playerId;
    
    QJsonArray cardArray;
    for (int card : cards) {
        cardArray.append(card);
    }
    msg["cards"] = cardArray;
    
    if (isHost) {
        BroadcastMessage(msg);
    } else if (clientSocket) {
        SendMessage(clientSocket, msg);
    }
}

void NetworkManager::SendPass(int playerId) {
    QJsonObject msg = CreateMessage(MSG_PASS);
    msg["playerId"] = playerId;
    
    if (isHost) {
        BroadcastMessage(msg);
    } else if (clientSocket) {
        SendMessage(clientSocket, msg);
    }
}

void NetworkManager::SendAIAction(int aiId, MessageType actionType, const QJsonObject& actionData) {
    if (!isHost) {
        qDebug() << "[NetworkManager] 只有房主可以发送AI操作";
        return;
    }
    
    QJsonObject msg = CreateMessage(MSG_AI_ACTION);
    msg["aiId"] = aiId;
    msg["actionType"] = (int)actionType;
    msg["actionData"] = actionData;
    
    BroadcastMessage(msg);
}

int NetworkManager::GetPlayerCount() const {
    if (!isConnected) return 0;
    return connections.size() + 1;  // +1 for local player
}

void NetworkManager::OnNewConnection() {
    if (!server) return;
    
    QTcpSocket* socket = server->nextPendingConnection();
    if (!socket) return;
    
    connect(socket, &QTcpSocket::disconnected, this, &NetworkManager::OnDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &NetworkManager::OnReadyRead);
    connect(socket, &QTcpSocket::errorOccurred, this, &NetworkManager::OnSocketError);
    
    // 分配玩家ID
    AssignPlayerId(socket);
    
    qDebug() << "[NetworkManager] 新玩家连接:" << socket->peerAddress().toString();
}

void NetworkManager::OnConnected() {
    qDebug() << "[NetworkManager] 成功连接到房主";
    isConnected = true;
    
    // 发送加入房间请求
    QJsonObject msg = CreateMessage(MSG_JOIN_ROOM);
    msg["playerName"] = "玩家" + QString::number(QDateTime::currentMSecsSinceEpoch() % 1000);
    SendMessage(clientSocket, msg);
}

void NetworkManager::OnDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    if (connections.contains(socket)) {
        int playerId = connections[socket].playerId;
        qDebug() << "[NetworkManager] 玩家" << playerId << "断开连接";
        
        connections.remove(socket);
        playerSockets.remove(playerId);
        
        emit playerLeft(playerId);
    }
    
    socket->deleteLater();
}

void NetworkManager::OnReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;
    
    while (socket->canReadLine()) {
        QByteArray data = socket->readLine();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        
        if (!doc.isObject()) {
            qDebug() << "[NetworkManager] 无效的JSON消息";
            continue;
        }
        
        ProcessMessage(socket, doc.object());
    }
}

void NetworkManager::OnSocketError(QAbstractSocket::SocketError error) {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        qDebug() << "[NetworkManager] Socket错误:" << socket->errorString();
        emit errorOccurred(socket->errorString());
    }
}

void NetworkManager::ProcessMessage(QTcpSocket* socket, const QJsonObject& message) {
    if (!message.contains("type")) {
        qDebug() << "[NetworkManager] 消息缺少type字段";
        return;
    }
    
    MessageType type = (MessageType)message["type"].toInt();
    
    switch (type) {
    case MSG_JOIN_ROOM:
        HandleJoinRoom(socket, message);
        break;
    case MSG_PLAYER_READY:
        HandlePlayerReady(socket, message);
        break;
    case MSG_GAME_START:
        HandleGameStart(socket, message);
        break;
    case MSG_GAME_STATE:
        HandleGameState(socket, message);
        break;
    case MSG_CALL_LANDLORD:
        HandleCallLandlord(socket, message);
        break;
    case MSG_DISCARD_CARDS:
        HandleDiscardCards(socket, message);
        break;
    case MSG_PASS:
        HandlePass(socket, message);
        break;
    case MSG_AI_ACTION:
        HandleAIAction(socket, message);
        break;
    case MSG_LEAVE_ROOM:
        OnDisconnected();
        break;
    default:
        qDebug() << "[NetworkManager] 未知消息类型:" << type;
        break;
    }
}

void NetworkManager::SendMessage(QTcpSocket* socket, const QJsonObject& message) {
    if (!socket || !socket->isValid()) return;
    
    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    socket->write(data);
    socket->flush();
}

void NetworkManager::BroadcastMessage(const QJsonObject& message) {
    for (auto socket : connections.keys()) {
        SendMessage(socket, message);
    }
}

QJsonObject NetworkManager::CreateMessage(MessageType type) {
    QJsonObject msg;
    msg["type"] = (int)type;
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

void NetworkManager::AssignPlayerId(QTcpSocket* socket) {
    PlayerConnection conn;
    conn.playerId = nextPlayerId++;
    conn.socket = socket;
    conn.isReady = false;
    
    connections[socket] = conn;
    playerSockets[conn.playerId] = socket;
    
    // 发送玩家ID给客户端
    QJsonObject msg = CreateMessage(MSG_PLAYER_INFO);
    msg["playerId"] = conn.playerId;
    msg["isHost"] = false;
    SendMessage(socket, msg);
    
    qDebug() << "[NetworkManager] 分配玩家ID:" << conn.playerId;
}

void NetworkManager::HandleJoinRoom(QTcpSocket* socket, const QJsonObject& message) {
    if (!isHost) return;
    
    QString playerName = message["playerName"].toString();
    
    if (connections.contains(socket)) {
        int playerId = connections[socket].playerId;
        connections[socket].playerName = playerName;
        
        qDebug() << "[NetworkManager] 玩家" << playerId << "加入房间:" << playerName;
        emit playerJoined(playerId, playerName);
        
        // 向该玩家发送当前所有玩家信息
        QJsonObject responseMsg = CreateMessage(MSG_PLAYER_INFO);
        responseMsg["playerId"] = playerId;
        responseMsg["isHost"] = false;
        SendMessage(socket, responseMsg);
    }
}

void NetworkManager::HandlePlayerReady(QTcpSocket* socket, const QJsonObject& message) {
    int playerId = message["playerId"].toInt();
    bool ready = message["ready"].toBool();
    
    if (connections.contains(socket)) {
        connections[socket].isReady = ready;
    }
    
    qDebug() << "[NetworkManager] 玩家" << playerId << "准备状态:" << ready;
    emit playerReadyChanged(playerId, ready);
    
    // 房主转发给其他玩家
    if (isHost) {
        BroadcastMessage(message);
    }
}

void NetworkManager::HandleGameStart(QTcpSocket* socket, const QJsonObject& message) {
    useAI = message["useAI"].toBool();
    gameStarted = true;
    
    qDebug() << "[NetworkManager] 收到游戏开始消息，使用AI:" << useAI;
    // 客户端发出gameStartRequested信号，触发界面准备，然后等待MSG_GAME_STATE同步游戏状态
    emit gameStartRequested();
}

void NetworkManager::HandleCallLandlord(QTcpSocket* socket, const QJsonObject& message) {
    int playerId = message["playerId"].toInt();
    int score = message["score"].toInt();
    
    qDebug() << "[NetworkManager] 收到叫地主消息: 玩家" << playerId << "叫分" << score;
    emit callLandlordReceived(playerId, score);
    
    // 房主转发给其他玩家
    if (isHost && connections.contains(socket)) {
        BroadcastMessage(message);
    }
}

void NetworkManager::HandleDiscardCards(QTcpSocket* socket, const QJsonObject& message) {
    int playerId = message["playerId"].toInt();
    QJsonArray cardArray = message["cards"].toArray();
    
    std::vector<int> cards;
    for (const QJsonValue& val : cardArray) {
        cards.push_back(val.toInt());
    }
    
    qDebug() << "[NetworkManager] 收到出牌消息: 玩家" << playerId << "出" << cards.size() << "张牌";
    emit discardCardsReceived(playerId, cards);
    
    // 房主转发给其他玩家
    if (isHost && connections.contains(socket)) {
        BroadcastMessage(message);
    }
}

void NetworkManager::HandlePass(QTcpSocket* socket, const QJsonObject& message) {
    int playerId = message["playerId"].toInt();
    
    qDebug() << "[NetworkManager] 收到过牌消息: 玩家" << playerId;
    emit passReceived(playerId);
    
    // 房主转发给其他玩家
    if (isHost && connections.contains(socket)) {
        BroadcastMessage(message);
    }
}

void NetworkManager::HandleAIAction(QTcpSocket* socket, const QJsonObject& message) {
    int aiId = message["aiId"].toInt();
    MessageType actionType = (MessageType)message["actionType"].toInt();
    QJsonObject actionData = message["actionData"].toObject();
    
    qDebug() << "[NetworkManager] 收到AI操作: AI" << aiId << "类型" << actionType;
    emit aiActionReceived(aiId, actionType, actionData);
}

void NetworkManager::BroadcastGameState(Game* game) {
    if (!isHost || !game) {
        qDebug() << "[NetworkManager] BroadcastGameState: 不是房主或game为空";
        return;
    }
    
    QJsonObject msg = CreateMessage(MSG_GAME_STATE);
    
    // 同步每个玩家的手牌
    for (int i = 0; i < 3; i++) {
        Player* player = game->GetPlayer(i);
        if (player) {
            QJsonArray handCards;
            const std::vector<int>& cards = player->GetHandCards();
            for (int card : cards) {
                handCards.append(card);
            }
            msg[QString("player%1Cards").arg(i)] = handCards;
        }
    }
    
    // 同步三张地主牌
    QJsonArray landlordCardsArray;
    for (int i = 0; i < 3; i++) {
        landlordCardsArray.append(game->GetLandlordCard(i));
    }
    msg["landlordCards"] = landlordCardsArray;
    
    // 同步游戏状态信息
    msg["status"] = (int)game->GetStatus();
    msg["currentPlayerId"] = game->GetCurrentPlayer() ? game->GetCurrentPlayer()->GetId() : -1;
    msg["useAI"] = useAI;
    
    qDebug() << "[NetworkManager] 广播游戏状态，当前玩家:" << msg["currentPlayerId"].toInt();
    BroadcastMessage(msg);
}

void NetworkManager::HandleGameState(QTcpSocket* socket, const QJsonObject& message) {
    qDebug() << "[NetworkManager] 收到游戏状态同步";
    
    // 发出信号，让外部处理游戏状态应用
    emit gameStateReceived(message);
    
    // 现在可以发出gameStartRequested信号了
    emit gameStartRequested();
}
