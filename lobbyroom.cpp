#include "lobbyroom.h"
#include "networkmanager.h"
#include <QMessageBox>
#include <QGroupBox>

LobbyRoom::LobbyRoom(QWidget* parent)
    : QWidget(parent), networkManager(nullptr), isReady(false) {
    SetupUI();
}

LobbyRoom::~LobbyRoom() {
}

void LobbyRoom::SetNetworkManager(NetworkManager* nm) {
    networkManager = nm;
    
    if (networkManager) {
        // 连接网络管理器的信号
        connect(networkManager, &NetworkManager::roomCreated, this, &LobbyRoom::OnRoomCreated);
        connect(networkManager, &NetworkManager::roomJoined, this, &LobbyRoom::OnRoomJoined);
        connect(networkManager, &NetworkManager::playerJoined, this, &LobbyRoom::OnPlayerJoined);
        connect(networkManager, &NetworkManager::playerLeft, this, &LobbyRoom::OnPlayerLeft);
        connect(networkManager, &NetworkManager::playerReadyChanged, this, &LobbyRoom::OnPlayerReadyChanged);
        connect(networkManager, &NetworkManager::gameStartRequested, this, &LobbyRoom::OnGameStartRequested);
        connect(networkManager, &NetworkManager::errorOccurred, this, &LobbyRoom::OnNetworkError);
    }
}

void LobbyRoom::SetupUI() {
    mainLayout = new QVBoxLayout(this);
    
    // ========== 大厅视图 ==========
    lobbyWidget = new QWidget(this);
    QVBoxLayout* lobbyLayout = new QVBoxLayout(lobbyWidget);
    
    QLabel* titleLabel = new QLabel("局域网联机", lobbyWidget);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    lobbyLayout->addWidget(titleLabel);
    
    lobbyLayout->addSpacing(30);
    
    // 创建房间按钮
    btnCreateRoom = new QPushButton("创建房间", lobbyWidget);
    btnCreateRoom->setMinimumHeight(50);
    btnCreateRoom->setStyleSheet("QPushButton { font-size: 16px; }");
    connect(btnCreateRoom, &QPushButton::clicked, this, &LobbyRoom::OnCreateRoomClicked);
    lobbyLayout->addWidget(btnCreateRoom);
    
    lobbyLayout->addSpacing(20);
    
    // 加入房间部分
    QGroupBox* joinGroup = new QGroupBox("加入房间", lobbyWidget);
    QVBoxLayout* joinLayout = new QVBoxLayout(joinGroup);
    
    QHBoxLayout* ipLayout = new QHBoxLayout();
    QLabel* labelIP = new QLabel("房主IP:", joinGroup);
    editHostIP = new QLineEdit(joinGroup);
    editHostIP->setPlaceholderText("192.168.1.100");
    editHostIP->setText("127.0.0.1");  // 默认本地测试
    ipLayout->addWidget(labelIP);
    ipLayout->addWidget(editHostIP);
    joinLayout->addLayout(ipLayout);
    
    QHBoxLayout* portLayout = new QHBoxLayout();
    QLabel* labelPort = new QLabel("端口:", joinGroup);
    editPort = new QLineEdit(joinGroup);
    editPort->setPlaceholderText("12345");
    editPort->setText("12345");  // 默认端口
    portLayout->addWidget(labelPort);
    portLayout->addWidget(editPort);
    joinLayout->addLayout(portLayout);
    
    btnJoinRoom = new QPushButton("加入", joinGroup);
    btnJoinRoom->setMinimumHeight(40);
    connect(btnJoinRoom, &QPushButton::clicked, this, &LobbyRoom::OnJoinRoomClicked);
    joinLayout->addWidget(btnJoinRoom);
    
    lobbyLayout->addWidget(joinGroup);
    
    lobbyLayout->addStretch();
    
    // 返回按钮
    btnBack = new QPushButton("返回", lobbyWidget);
    btnBack->setMinimumHeight(40);
    connect(btnBack, &QPushButton::clicked, this, &LobbyRoom::OnBackClicked);
    lobbyLayout->addWidget(btnBack);
    
    mainLayout->addWidget(lobbyWidget);
    
    // ========== 房间视图 ==========
    roomWidget = new QWidget(this);
    QVBoxLayout* roomLayout = new QVBoxLayout(roomWidget);
    
    labelRoomInfo = new QLabel("房间信息", roomWidget);
    QFont roomFont = labelRoomInfo->font();
    roomFont.setPointSize(16);
    labelRoomInfo->setFont(roomFont);
    labelRoomInfo->setAlignment(Qt::AlignCenter);
    roomLayout->addWidget(labelRoomInfo);
    
    roomLayout->addSpacing(20);
    
    // 玩家列表
    QLabel* playerLabel = new QLabel("玩家列表:", roomWidget);
    roomLayout->addWidget(playerLabel);
    
    playerListWidget = new QListWidget(roomWidget);
    playerListWidget->setMinimumHeight(200);
    roomLayout->addWidget(playerListWidget);
    
    roomLayout->addSpacing(20);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    btnReady = new QPushButton("准备", roomWidget);
    btnReady->setMinimumHeight(50);
    btnReady->setStyleSheet("QPushButton { font-size: 14px; }");
    connect(btnReady, &QPushButton::clicked, this, &LobbyRoom::OnReadyClicked);
    buttonLayout->addWidget(btnReady);
    
    btnStartGame = new QPushButton("开始游戏", roomWidget);
    btnStartGame->setMinimumHeight(50);
    btnStartGame->setStyleSheet("QPushButton { font-size: 14px; }");
    btnStartGame->setEnabled(false);  // 默认禁用，只有房主可用
    connect(btnStartGame, &QPushButton::clicked, this, &LobbyRoom::OnStartGameClicked);
    buttonLayout->addWidget(btnStartGame);
    
    roomLayout->addLayout(buttonLayout);
    
    btnLeaveRoom = new QPushButton("离开房间", roomWidget);
    btnLeaveRoom->setMinimumHeight(40);
    connect(btnLeaveRoom, &QPushButton::clicked, this, &LobbyRoom::OnBackClicked);
    roomLayout->addWidget(btnLeaveRoom);
    
    mainLayout->addWidget(roomWidget);
    
    // 初始显示大厅视图
    ShowLobbyView();
    
    setMinimumSize(600, 500);
}

void LobbyRoom::ShowLobbyView() {
    lobbyWidget->setVisible(true);
    roomWidget->setVisible(false);
    isReady = false;
}

void LobbyRoom::ShowRoomView() {
    lobbyWidget->setVisible(false);
    roomWidget->setVisible(true);
}

void LobbyRoom::OnCreateRoomClicked() {
    if (!networkManager) {
        QMessageBox::warning(this, "错误", "网络管理器未初始化");
        return;
    }
    
    int port = editPort->text().isEmpty() ? 12345 : editPort->text().toInt();
    if (networkManager->CreateRoom(port)) {
        // 成功创建，等待OnRoomCreated回调
    }
}

void LobbyRoom::OnJoinRoomClicked() {
    if (!networkManager) {
        QMessageBox::warning(this, "错误", "网络管理器未初始化");
        return;
    }
    
    QString hostIP = editHostIP->text();
    int port = editPort->text().isEmpty() ? 12345 : editPort->text().toInt();
    
    if (hostIP.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入房主IP地址");
        return;
    }
    
    if (networkManager->JoinRoom(hostIP, port)) {
        // 成功发起连接，等待OnRoomJoined回调
    }
}

void LobbyRoom::OnBackClicked() {
    if (networkManager && networkManager->IsConnected()) {
        networkManager->LeaveRoom();
    }
    
    ShowLobbyView();
    emit backToMenu();
}

void LobbyRoom::OnReadyClicked() {
    if (!networkManager) return;
    
    isReady = !isReady;
    networkManager->SetPlayerReady(isReady);
    
    btnReady->setText(isReady ? "取消准备" : "准备");
    btnReady->setStyleSheet(isReady ? 
        "QPushButton { font-size: 14px; background-color: #4CAF50; color: white; }" :
        "QPushButton { font-size: 14px; }");
}

void LobbyRoom::OnStartGameClicked() {
    if (!networkManager || !networkManager->IsHost()) {
        QMessageBox::warning(this, "错误", "只有房主可以开始游戏");
        return;
    }
    
    int playerCount = networkManager->GetPlayerCount();
    if (playerCount < 2 || playerCount > 3) {
        QMessageBox::warning(this, "错误", "需要2-3名玩家才能开始游戏");
        return;
    }
    
    networkManager->StartGame();
}

void LobbyRoom::OnRoomCreated(int port) {
    ShowRoomView();
    labelRoomInfo->setText(QString("房间已创建 - 端口: %1\n等待其他玩家加入...").arg(port));
    btnStartGame->setEnabled(true);  // 房主可以开始游戏
    
    // 添加自己到玩家列表
    playerListWidget->clear();
    playerListWidget->addItem("玩家0 (房主-我) [准备]");
    
    QMessageBox::information(this, "成功", 
        QString("房间创建成功！\n其他玩家可通过以下信息加入:\nIP: (您的局域网IP)\n端口: %1").arg(port));
}

void LobbyRoom::OnRoomJoined() {
    ShowRoomView();
    labelRoomInfo->setText("已加入房间\n等待房主开始游戏...");
    btnStartGame->setEnabled(false);  // 非房主不能开始
    
    UpdatePlayerList();
}

void LobbyRoom::OnPlayerJoined(int playerId, const QString& playerName) {
    UpdatePlayerList();
    
    if (networkManager && networkManager->IsHost()) {
        labelRoomInfo->setText(QString("房间 - 玩家数: %1/3").arg(networkManager->GetPlayerCount()));
    }
}

void LobbyRoom::OnPlayerLeft(int playerId) {
    UpdatePlayerList();
    
    if (networkManager && networkManager->IsHost()) {
        labelRoomInfo->setText(QString("房间 - 玩家数: %1/3").arg(networkManager->GetPlayerCount()));
    }
}

void LobbyRoom::OnPlayerReadyChanged(int playerId, bool ready) {
    UpdatePlayerList();
}

void LobbyRoom::OnGameStartRequested() {
    emit gameStarting();
}

void LobbyRoom::OnNetworkError(const QString& error) {
    QMessageBox::warning(this, "网络错误", error);
}

void LobbyRoom::UpdatePlayerList() {
    if (!networkManager) return;
    
    playerListWidget->clear();
    
    // 添加本地玩家
    int localId = networkManager->GetLocalPlayerId();
    QString localText = QString("玩家%1 (我)").arg(localId);
    if (networkManager->IsHost()) {
        localText += " [房主]";
    }
    if (isReady || networkManager->IsHost()) {
        localText += " [准备]";
    }
    playerListWidget->addItem(localText);
    
    // TODO: 添加其他玩家（需要从NetworkManager获取）
    // 这里简化处理，实际应该维护玩家列表
    int playerCount = networkManager->GetPlayerCount();
    for (int i = 0; i < playerCount; ++i) {
        if (i != localId) {
            playerListWidget->addItem(QString("玩家%1").arg(i));
        }
    }
}
