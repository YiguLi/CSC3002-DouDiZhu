#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

/**
 * 网络消息类型
 */
enum class MessageType {
    // 连接相关
    PlayerJoin,          // 玩家加入
    PlayerLeave,         // 玩家离开
    PlayerList,          // 玩家列表更新
    GameStart,           // 游戏开始
    
    // 游戏流程
    DealCards,           // 发牌
    CallLandlord,        // 叫地主
    LandlordConfirm,     // 地主确定
    SendLandlordCards,   // 发地主牌
    
    // 出牌相关
    PlayerDiscard,       // 玩家出牌
    PlayerPass,          // 玩家过牌
    TurnChange,          // 回合切换
    
    // 游戏结束
    GameOver,            // 游戏结束
    
    // 错误和状态
    Error,               // 错误消息
    Ping,                // 心跳包
    Pong                 // 心跳响应
};

/**
 * 游戏服务器类
 * 作为房主运行，管理所有客户端连接
 */
class GameServer : public QObject
{
    Q_OBJECT
public:
    explicit GameServer(QObject *parent = nullptr);
    ~GameServer();
    
    bool startServer(quint16 port = 12345);
    void stopServer();
    bool isRunning() const { return m_server && m_server->isListening(); }
    QString getLocalIP() const;
    
    void broadcastMessage(const QJsonObject& message);
    void sendToClient(int playerId, const QJsonObject& message);
    
    int getConnectedPlayerCount() const { return m_clients.size(); }
    QStringList getPlayerNames() const;

signals:
    void playerJoined(int playerId, const QString& playerName);
    void playerLeft(int playerId);
    void messageReceived(int playerId, MessageType type, const QJsonObject& data);
    void serverError(const QString& error);

private slots:
    void onNewConnection();
    void onClientReadyRead();
    void onClientDisconnected();

private:
    QTcpServer* m_server;
    QMap<int, QTcpSocket*> m_clients;     // playerId -> socket
    QMap<QTcpSocket*, int> m_socketToId;  // socket -> playerId
    QMap<int, QString> m_playerNames;     // playerId -> name
    int m_nextPlayerId;
    
    void processMessage(int playerId, const QJsonObject& message);
    QJsonObject createMessage(MessageType type, const QJsonObject& data = QJsonObject());
};

/**
 * 游戏客户端类
 * 作为普通玩家连接到服务器
 */
class GameClient : public QObject
{
    Q_OBJECT
public:
    explicit GameClient(QObject *parent = nullptr);
    ~GameClient();
    
    void connectToServer(const QString& ip, quint16 port = 12345);
    void disconnectFromServer();
    bool isConnected() const { return m_socket && m_socket->state() == QTcpSocket::ConnectedState; }
    
    void sendMessage(const QJsonObject& message);
    void setPlayerName(const QString& name) { m_playerName = name; }
    QString getPlayerName() const { return m_playerName; }
    int getMyPlayerId() const { return m_myPlayerId; }

signals:
    void connected();
    void disconnected();
    void messageReceived(MessageType type, const QJsonObject& data);
    void connectionError(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket* m_socket;
    QString m_playerName;
    int m_myPlayerId;
    
    void processMessage(const QJsonObject& message);
    QJsonObject createMessage(MessageType type, const QJsonObject& data = QJsonObject());
};

// 辅助函数：消息类型转换
QString messageTypeToString(MessageType type);
MessageType stringToMessageType(const QString& str);

#endif // NETWORKMANAGER_H
