#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QMap>
#include <vector>

class Game;

/**
 * 网络消息类型
 */
enum MessageType {
    // 房间管理
    MSG_CREATE_ROOM,        // 创建房间
    MSG_JOIN_ROOM,          // 加入房间
    MSG_LEAVE_ROOM,         // 离开房间
    MSG_PLAYER_READY,       // 玩家准备
    MSG_GAME_START,         // 游戏开始
    
    // 游戏同步
    MSG_CALL_LANDLORD,      // 叫地主
    MSG_DISCARD_CARDS,      // 出牌
    MSG_PASS,               // 过牌
    MSG_AI_ACTION,          // AI操作（房主发送）
    
    // 游戏状态
    MSG_GAME_STATE,         // 游戏状态同步
    MSG_PLAYER_INFO,        // 玩家信息
    MSG_ERROR               // 错误消息
};

/**
 * 玩家连接信息
 */
struct PlayerConnection {
    int playerId;
    QString playerName;
    QTcpSocket* socket;
    bool isReady;
    
    PlayerConnection() : playerId(-1), socket(nullptr), isReady(false) {}
};

/**
 * 网络管理器
 * 负责局域网通信、房间管理、消息同步
 */
class NetworkManager : public QObject {
    Q_OBJECT
    
public:
    explicit NetworkManager(QObject* parent = nullptr);
    ~NetworkManager();
    
    // 房间管理
    bool CreateRoom(int port = 12345);           // 创建房间（作为房主）
    bool JoinRoom(const QString& hostAddress, int port = 12345);  // 加入房间
    void LeaveRoom();                             // 离开房间
    void SetPlayerReady(bool ready);              // 设置本地玩家准备状态
    void StartGame();                             // 开始游戏（仅房主）
    
    // 游戏操作同步
    void BroadcastGameState(Game* game);              // 广播游戏状态（仅房主）
    void SendCallLandlord(int playerId, int score);      // 发送叫地主
    void SendDiscardCards(int playerId, const std::vector<int>& cards);  // 发送出牌
    void SendPass(int playerId);                  // 发送过牌
    void SendAIAction(int aiId, MessageType actionType, const QJsonObject& actionData);  // 发送AI操作
    
    // 状态查询
    bool IsHost() const { return isHost; }
    bool IsConnected() const { return isConnected; }
    bool IsGameStarted() const { return gameStarted; }
    int GetLocalPlayerId() const { return localPlayerId; }
    int GetPlayerCount() const;
    
    // 游戏实例设置
    void SetGame(Game* g) { game = g; }
    
signals:
    // 房间事件
    void roomCreated(int port);
    void roomJoined();
    void playerJoined(int playerId, const QString& playerName);
    void playerLeft(int playerId);
    void playerReadyChanged(int playerId, bool ready);
    void gameStartRequested();
    
    // 游戏事件
    void gameStateReceived(const QJsonObject& gameState);  // 接收游戏状态
    void callLandlordReceived(int playerId, int score);
    void discardCardsReceived(int playerId, const std::vector<int>& cards);
    void passReceived(int playerId);
    void aiActionReceived(int aiId, MessageType actionType, const QJsonObject& actionData);
    
    // 错误事件
    void errorOccurred(const QString& error);
    
private slots:
    void OnNewConnection();                      // 新客户端连接（服务器端）
    void OnConnected();                          // 连接成功（客户端）
    void OnDisconnected();                       // 断开连接
    void OnReadyRead();                          // 接收到数据
    void OnSocketError(QAbstractSocket::SocketError error);  // Socket错误
    
private:
    void ProcessMessage(QTcpSocket* socket, const QJsonObject& message);  // 处理接收到的消息
    void SendMessage(QTcpSocket* socket, const QJsonObject& message);     // 发送消息
    void BroadcastMessage(const QJsonObject& message);                    // 广播消息到所有客户端
    QJsonObject CreateMessage(MessageType type);                          // 创建消息基础结构
    
    void AssignPlayerId(QTcpSocket* socket);     // 为新连接分配玩家ID
    void HandleCreateRoom(QTcpSocket* socket, const QJsonObject& message);
    void HandleJoinRoom(QTcpSocket* socket, const QJsonObject& message);
    void HandlePlayerReady(QTcpSocket* socket, const QJsonObject& message);
    void HandleGameStart(QTcpSocket* socket, const QJsonObject& message);
    void HandleGameState(QTcpSocket* socket, const QJsonObject& message);
    void HandleCallLandlord(QTcpSocket* socket, const QJsonObject& message);
    void HandleDiscardCards(QTcpSocket* socket, const QJsonObject& message);
    void HandlePass(QTcpSocket* socket, const QJsonObject& message);
    void HandleAIAction(QTcpSocket* socket, const QJsonObject& message);
    
    Game* game;                                  // 游戏实例
    QTcpServer* server;                          // TCP服务器（房主模式）
    QTcpSocket* clientSocket;                    // 客户端连接（加入房间模式）
    
    bool isHost;                                 // 是否是房主
    bool isConnected;                            // 是否已连接
    bool gameStarted;                            // 游戏是否已开始
    int localPlayerId;                           // 本地玩家ID
    
    QMap<QTcpSocket*, PlayerConnection> connections;  // Socket到玩家的映射
    QMap<int, QTcpSocket*> playerSockets;             // 玩家ID到Socket的映射
    
    int nextPlayerId;                            // 下一个要分配的玩家ID
    bool useAI;                                  // 是否使用AI（二人联机模式）
};

#endif // NETWORKMANAGER_H
