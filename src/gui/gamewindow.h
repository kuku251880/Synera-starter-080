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

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    void fitSceneInView();

    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    QGraphicsView* m_view;
    QPushButton* m_resetButton;
    Game* m_game;
};

#endif // GAMEWINDOW_H
