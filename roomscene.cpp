#include "roomscene.h"
#include "ui_roomscene.h"
#include "ingamescene.h"
#include <QMessageBox>
#include <QJsonArray>

RoomScene::RoomScene(bool isServer, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RoomScene)
    , m_isServer(isServer)
    , m_server(nullptr)
    , m_client(nullptr)
{
    ui->setupUi(this);
    this->setWindowTitle(isServer ? "游戏房间 (房主)" : "游戏房间 (客户端)");
    this->setFixedSize(800, 600);
    
    if (m_isServer) {
        // 创建服务器
        m_server = new GameServer(this);
        
        connect(m_server, &GameServer::playerJoined, this, &RoomScene::onPlayerJoined);
        connect(m_server, &GameServer::playerLeft, this, &RoomScene::onPlayerLeft);
        connect(m_server, &GameServer::serverError, this, &RoomScene::onServerError);
        connect(m_server, &GameServer::messageReceived, this, &RoomScene::onServerMessageReceived);
        
        if (m_server->startServer()) {
            QString ip = m_server->getLocalIP();
            ui->ipLabel->setText(QString("📡 房间IP: %1:12345  (请告诉其他玩家此IP地址)").arg(ip));
            ui->statusLabel->setText("状态: 等待玩家加入... (至少需要2名玩家)");
            
            // 房主自己是玩家0
            m_players[0] = "房主(你)";
            updatePlayerList();
            
            // 显示操作提示
            QMessageBox::information(this, "✓ 房间已创建", 
                QString("🎮 房间创建成功！\n\n"
                        "你的局域网IP地址是:\n"
                        "【 %1 】\n"
                        "端口: 12345\n\n"
                        "📋 操作步骤:\n"
                        "1. 将上面的IP地址告诉其他玩家\n"
                        "2. 其他玩家点击「加入房间」并输入此IP\n"
                        "3. 至少2名玩家后，你可以点击「开始游戏」\n\n"
                        "💡 提示: 所有玩家必须在同一WiFi网络内").arg(ip));
        } else {
            QMessageBox::critical(this, "错误", "❌ 无法创建服务器!\n\n可能的原因:\n- 端口12345被占用\n- 网络权限不足");
            this->close();
        }
    } else {
        // 创建客户端
        m_client = new GameClient(this);
        
        connect(m_client, &GameClient::connected, this, &RoomScene::onClientConnected);
        connect(m_client, &GameClient::disconnected, this, &RoomScene::onClientDisconnected);
        connect(m_client, &GameClient::connectionError, this, &RoomScene::onClientError);
        connect(m_client, &GameClient::messageReceived, this, &RoomScene::onClientMessageReceived);
        
        m_client->setPlayerName("玩家");
        ui->ipLabel->setText("正在连接...");
        ui->statusLabel->setText("状态: 正在连接到服务器...");
        
        // 只有房主能开始游戏
        ui->startGameBtn->setVisible(false);
    }
}

RoomScene::~RoomScene()
{
    if (m_server) {
        m_server->stopServer();
    }
    if (m_client) {
        m_client->disconnectFromServer();
    }
    delete ui;
}

void RoomScene::connectToServer(const QString& ip)
{
    if (m_client) {
        m_client->connectToServer(ip);
    }
}

void RoomScene::updatePlayerList()
{
    QStringList playerLabels = {
        ui->player1Label->text(),
        ui->player2Label->text(),
        ui->player3Label->text()
    };
    
    // 重置所有标签
    ui->player1Label->setText("玩家1: 等待中...");
    ui->player2Label->setText("玩家2: 等待中...");
    ui->player3Label->setText("玩家3: 等待中...");
    
    // 更新玩家信息
    QList<int> playerIds = m_players.keys();
    std::sort(playerIds.begin(), playerIds.end());
    
    for (int i = 0; i < playerIds.size() && i < 3; ++i) {
        int playerId = playerIds[i];
        QString playerName = m_players[playerId];
        QString text = QString("玩家%1: %2 %3")
                           .arg(i + 1)
                           .arg(playerName)
                           .arg(playerId == 0 ? "(房主)" : "");
        
        switch (i) {
            case 0: ui->player1Label->setText(text); break;
            case 1: ui->player2Label->setText(text); break;
            case 2: ui->player3Label->setText(text); break;
        }
    }
    
    updateStartButton();
}

void RoomScene::updateStartButton()
{
    if (m_isServer) {
        // 至少需要2个玩家才能开始（可以2人+1AI或3人）
        int playerCount = m_players.size();
        ui->startGameBtn->setEnabled(playerCount >= 2);
        
        if (playerCount >= 2) {
            ui->statusLabel->setText(QString("状态: 已有%1名玩家，可以开始游戏").arg(playerCount));
        } else {
            ui->statusLabel->setText("状态: 等待更多玩家加入...");
        }
    }
}

void RoomScene::on_startGameBtn_clicked()
{
    if (!m_isServer || m_players.size() < 2) {
        return;
    }
    
    // 发送游戏开始消息给所有客户端
    QJsonObject data;
    data["playerCount"] = m_players.size();
    
    QJsonArray playerArray;
    for (auto it = m_players.constBegin(); it != m_players.constEnd(); ++it) {
        QJsonObject player;
        player["playerId"] = it.key();
        player["playerName"] = it.value();
        playerArray.append(player);
    }
    data["players"] = playerArray;
    
    QJsonObject message;
    message["type"] = messageTypeToString(MessageType::GameStart);
    message["data"] = data;
    
    m_server->broadcastMessage(message);
    
    // 启动网络游戏
    startNetworkGame();
}

void RoomScene::on_backBtn_clicked()
{
    QString message = m_isServer 
        ? "确定要关闭房间吗？\n\n所有玩家将被断开连接。"
        : "确定要离开房间吗？";
    
    auto reply = QMessageBox::question(this, "确认退出", message,
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (m_server) {
            m_server->stopServer();
        }
        if (m_client) {
            m_client->disconnectFromServer();
        }
        this->close();
    }
}

// ========== 服务器相关槽函数 ==========

void RoomScene::onPlayerJoined(int playerId, const QString& playerName)
{
    m_players[playerId] = playerName;
    updatePlayerList();
    
    ui->statusLabel->setText(QString("状态: 玩家 %1 加入了房间").arg(playerName));
}

void RoomScene::onPlayerLeft(int playerId)
{
    QString playerName = m_players.take(playerId);
    updatePlayerList();
    
    ui->statusLabel->setText(QString("状态: 玩家 %1 离开了房间").arg(playerName));
}

void RoomScene::onServerError(const QString& error)
{
    QMessageBox::critical(this, "服务器错误", error);
}

void RoomScene::onServerMessageReceived(int playerId, MessageType type, const QJsonObject& data)
{
    // 处理来自客户端的消息
    // 目前在房间阶段不需要处理太多消息
}

// ========== 客户端相关槽函数 ==========

void RoomScene::onClientConnected()
{
    ui->ipLabel->setText("✓ 已成功连接到房间");
    ui->statusLabel->setText("状态: 等待房主开始游戏...");
    
    QMessageBox::information(this, "✓ 连接成功", 
        "🎉 已成功加入房间！\n\n"
        "请等待房主开始游戏...\n"
        "当房间内有至少2名玩家时，房主即可开始游戏。");
}

void RoomScene::onClientDisconnected()
{
    QMessageBox::information(this, "提示", "已断开与服务器的连接");
    this->close();
}

void RoomScene::onClientError(const QString& error)
{
    QMessageBox::critical(this, "❌ 连接失败", 
        QString("无法连接到房间:\n%1\n\n"
                "🔍 请检查:\n"
                "✓ IP地址是否正确\n"
                "✓ 房主是否已创建房间\n"
                "✓ 是否在同一WiFi网络内\n"
                "✓ 防火墙是否允许连接(端口12345)").arg(error));
    this->close();
}

void RoomScene::onClientMessageReceived(MessageType type, const QJsonObject& data)
{
    if (type == MessageType::PlayerList) {
        // 更新玩家列表
        m_players.clear();
        QJsonArray players = data["players"].toArray();
        
        for (const QJsonValue& val : players) {
            QJsonObject player = val.toObject();
            int playerId = player["playerId"].toInt();
            QString playerName = player["playerName"].toString();
            m_players[playerId] = playerName;
        }
        
        updatePlayerList();
    }
    else if (type == MessageType::GameStart) {
        // 游戏开始
        startNetworkGame();
    }
}

void RoomScene::startNetworkGame()
{
    // 创建网络游戏场景
    InGameScene *gameScene = new InGameScene(m_isServer ? m_server : nullptr, 
                                              m_isServer ? nullptr : m_client,
                                              this);
    gameScene->show();
    this->hide();
}
