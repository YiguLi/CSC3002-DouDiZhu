#ifndef MODESELECTSCENE_H
#define MODESELECTSCENE_H

#include <QDialog>

namespace Ui {
class ModeSelectScene;
}

class ModeSelectScene : public QDialog
{
    Q_OBJECT

public:
    explicit ModeSelectScene(QWidget *parent = nullptr);
    ~ModeSelectScene();

private slots:
    void on_singlePlayerBtn_clicked();
    void on_multiPlayerBtn_clicked();
    void on_backBtn_clicked();

private:
    Ui::ModeSelectScene *ui;
};

#endif // MODESELECTSCENE_H
