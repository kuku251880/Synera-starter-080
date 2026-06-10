#ifndef GAMEWINDOW_H
#define GAMEWINDOW_H

#include <QMainWindow>

class Game;
class QGraphicsView;
class QPushButton;
class QVBoxLayout;
class QResizeEvent;

class GameWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GameWindow(QWidget* parent = nullptr);
    ~GameWindow();

private slots:
    void onResetButtonClicked();
    void onStartCombatButtonClicked();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    void fitSceneInView();
    void showShopDialog();
    void showArchiveDialog(bool loadMode);

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QGraphicsView* m_view;
    QPushButton* m_resetButton;
    QPushButton* m_startCombatButton;
    QPushButton* m_shopButton;
    QPushButton* m_levelUpButton;
    QPushButton* m_equipButton;
    QPushButton* m_saveButton;
    QPushButton* m_loadButton;
    Game* m_game;
};

#endif // GAMEWINDOW_H
