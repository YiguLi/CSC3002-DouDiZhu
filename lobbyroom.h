#ifndef LOBBYROOM_H
#define LOBBYROOM_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

class NetworkManager;

/**
 * 联机大厅/房间界面
 */
class LobbyRoom : public QWidget {
    Q_OBJECT
    
public:
    explicit LobbyRoom(QWidget* parent = nullptr);
    ~LobbyRoom();
    
    void SetNetworkManager(NetworkManager* nm);
    NetworkManager* GetNetworkManager() const { return networkManager; }
    
signals:
    void gameStarting();          // 游戏即将开始
    void backToMenu();            // 返回主菜单
    
private slots:
    void OnCreateRoomClicked();
    void OnJoinRoomClicked();
    void OnBackClicked();
    void OnReadyClicked();
    void OnStartGameClicked();
    
    void OnRoomCreated(int port);
    void OnRoomJoined();
    void OnPlayerJoined(int playerId, const QString& playerName);
    void OnPlayerLeft(int playerId);
    void OnPlayerReadyChanged(int playerId, bool ready);
    void OnGameStartRequested();
    void OnNetworkError(const QString& error);
    
private:
    void SetupUI();
    void ShowLobbyView();          // 显示大厅视图（创建/加入房间）
    void ShowRoomView();           // 显示房间视图（等待开始）
    void UpdatePlayerList();
    
    NetworkManager* networkManager;
    
    // 大厅视图控件
    QWidget* lobbyWidget;
    QPushButton* btnCreateRoom;
    QPushButton* btnJoinRoom;
    QLineEdit* editHostIP;
    QLineEdit* editPort;
    QPushButton* btnBack;
    
    // 房间视图控件
    QWidget* roomWidget;
    QLabel* labelRoomInfo;
    QListWidget* playerListWidget;
    QPushButton* btnReady;
    QPushButton* btnStartGame;
    QPushButton* btnLeaveRoom;
    
    QVBoxLayout* mainLayout;
    
    bool isReady;
};

#endif // LOBBYROOM_H
