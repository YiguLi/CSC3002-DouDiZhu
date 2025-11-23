#ifndef ROOMSCENE_H
#define ROOMSCENE_H

#include <QDialog>
#include "networkmanager.h"

namespace Ui {
class RoomScene;
}

class RoomScene : public QDialog
{
    Q_OBJECT

public:
    explicit RoomScene(bool isServer, QWidget *parent = nullptr);
    ~RoomScene();
    
    void connectToServer(const QString& ip);

private slots:
    void on_startGameBtn_clicked();
    void on_backBtn_clicked();
    
    // 服务器相关槽
    void onPlayerJoined(int playerId, const QString& playerName);
    void onPlayerLeft(int playerId);
    void onServerError(const QString& error);
    void onServerMessageReceived(int playerId, MessageType type, const QJsonObject& data);
    
    // 客户端相关槽
    void onClientConnected();
    void onClientDisconnected();
    void onClientError(const QString& error);
    void onClientMessageReceived(MessageType type, const QJsonObject& data);

private:
    Ui::RoomScene *ui;
    bool m_isServer;
    GameServer* m_server;
    GameClient* m_client;
    
    QMap<int, QString> m_players; // playerId -> playerName
    
    void updatePlayerList();
    void updateStartButton();
    void startNetworkGame();
};

#endif // ROOMSCENE_H
