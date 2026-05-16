#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);
        ~MainWindow() override;

        float getCpuUsage();
        float getRamUsage();
        float calculateCpuUsage(int pid, long long deltatCpuTotal );

    public slots :
        void majSystemInfo();
        void loadProcess();


    private:
        Ui::MainWindow *ui;
        long long m_prevTotal = 0 ;
        long long m_prevIdle = 0 ;
        long long m_lastDeltaTotal = 0 ;
        QMap<int,long long> previousProcessTime ;

};
#endif // MAINWINDOW_H
