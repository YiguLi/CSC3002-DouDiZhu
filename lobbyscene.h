#ifndef LOBBYSCENE_H
#define LOBBYSCENE_H

#include <QDialog>

namespace Ui {
class LobbyScene;
}

class LobbyScene : public QDialog
{
    Q_OBJECT

public:
    explicit LobbyScene(QWidget *parent = nullptr);
    ~LobbyScene();

private slots:
    void on_createRoomBtn_clicked();
    void on_joinRoomBtn_clicked();
    void on_backBtn_clicked();

private:
    Ui::LobbyScene *ui;
};

#endif // LOBBYSCENE_H
