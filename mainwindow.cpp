#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QHeaderView>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QDir>
#include <QFileInfoList>

using namespace std;

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Ajout du timer 1s
        QTimer *timer = new QTimer(this) ;

        connect(timer , &QTimer::timeout , this , &MainWindow::majSystemInfo ) ;
        timer->start(1000);

    //Ajustement Automatique des colonnes
    ui->processTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    getCpuUsage() ; // Premier appel pour initialiser m_prevTotal et m_prevIdle

    // Masques les 2 cards pas encore términées
    ui->swapCard->setVisible(false) ;
    ui->gpuCard->setVisible(false) ;
}

MainWindow::~MainWindow()
{
    delete ui;
}


//-------------------------------------------------------------------

float MainWindow::getCpuUsage()
{
    QFile file("/proc/stat");
    QString ligne ;
    QStringList liste ;
    long long user , nice , system , idle , total ;
    long long deltatTotal , deltatIdle ;
    float cpuUsage ;

    if(! file.open(QIODevice::ReadOnly | QIODevice::Text) )
    {
        return 0 ;
    }

    QTextStream in(&file);
    ligne = in.readLine() ;
    liste = ligne.split(" " , Qt::SkipEmptyParts ) ;
    file.close() ;

    user   = liste[1].toLongLong();
    nice   = liste[2].toLongLong();
    system = liste[3].toLongLong();
    idle   = liste[4].toLongLong();

    total = user + nice + system + idle ;

    deltatTotal = total - m_prevTotal ;
    deltatIdle  = idle  - m_prevIdle ;

    cpuUsage = (1 - ((float)deltatIdle / deltatTotal )) * 100.0 ;

    m_prevTotal = total ;
    m_prevIdle  = idle ;

    m_lastDeltaTotal = deltatTotal ;

    return cpuUsage ;
}

//-------------------------------------------------------------------

float MainWindow::getRamUsage()
{
    QString contenu ;
    QStringList lignes ;
    long long memTotal = 0 , memAvailable = 0 ;
    float memUsage = 0.0f , memPercent ;

    QFile file("/proc/meminfo");
    if(! file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(this , "Erreur" , "Impossible de lire le fichier /proc/meminfo .") ;
        return 0.0f;
    }

    contenu = file.readAll() ;
    lignes = contenu.split("\n");

    for(const QString &ligne : as_const(lignes) )
    {
        if(ligne.startsWith("MemTotal:"))
        {
            memTotal = ligne.split(" " , Qt::SkipEmptyParts )[1].toLongLong() ;
        }

        if(ligne.startsWith("MemAvailable:"))
        {
            memAvailable = ligne.split(" ", Qt::SkipEmptyParts)[1].toLongLong();
        }

        if(memTotal != 0 && memAvailable != 0 )
        {
            break ;
        }
    }

    if(memTotal == 0)
    {
        return 0.0f;
    }

    memUsage   = float(memTotal - memAvailable) ;
    memPercent = memUsage / memTotal * 100 ;

    return (memPercent) ;
}

//-------------------------------------------------------------------
void MainWindow::majSystemInfo()
{
    float cpuUsage ;
    float ramUsage ;

    cpuUsage = getCpuUsage() ;
    ramUsage = getRamUsage() ;

    progressBarMaj(cpuUsage , ramUsage );

    ui->cpuPercent->setText(QString::number(cpuUsage , 'f' , 1 ) + "%") ;
    ui->cpuProgressBar->setValue((int)cpuUsage) ;

    ui->ramPercent->setText(QString::number(ramUsage , 'f' , 1 ) + "%") ;
    ui->ramProgressBar->setValue((int)ramUsage) ;

    // Desactivation du tri autom avant remplissage
    ui->processTable->setSortingEnabled(false) ;
    loadProcess() ;
    ui->processTable->setSortingEnabled(true) ; // Activation du tri

}

//---------------------------------------------------------------------

void MainWindow::progressBarMaj(float cpu, float ram)
{
    QString cpuState;
    QString ramState;

    // --- Logique pour le CPU ---
    if (cpu >= 80)
    {
        if( !cpuAlert )
        {
            QMessageBox::warning(this , "Alerte système" , "Utilisation CPU critique !" ) ;

            cpuAlert = true ;
        }
        else
        {
            cpuAlert = false ;
        }

        cpuState = "critical";
    }
    else if(cpu >= 60)
    {
        cpuState = "warning";
    }
    else
    {
        cpuState = "normal";
    }

    // --- Logique pour la RAM ---
    if (ram >= 90)
    {
        if(!ramAlert)
        {
            QMessageBox::critical(this , "Alerte système" , "Utilisation RAM critique !");

            ramAlert = true ;
        }
        else
        {
            ramAlert = false ;
        }

        ramState = "critical";
    }
    else if (ram >= 70)
    {
        ramState = "warning";
    }
    else
    {
        ramState = "normal";
    }

    // --- Application et rafraîchissement pour le CPU ---
    if (ui->cpuProgressBar->property("state").toString() != cpuState)
    {
        ui->cpuProgressBar->setProperty("state", cpuState);
        ui->cpuProgressBar->style()->unpolish(ui->cpuProgressBar);
        ui->cpuProgressBar->style()->polish(ui->cpuProgressBar);

        ui->cpuPercent->setProperty("state", cpuState);

        ui->cpuProgressBar->style()->unpolish(ui->cpuProgressBar);
        ui->cpuProgressBar->style()->polish(ui->cpuProgressBar);

        ui->cpuPercent->style()->unpolish(ui->cpuPercent);
        ui->cpuPercent->style()->polish(ui->cpuPercent);

    }

    // --- Application et rafraîchissement pour la RAM ---
    if (ui->ramProgressBar->property("state").toString() != ramState)
    {
        ui->ramProgressBar->setProperty("state", ramState);

        ui->ramProgressBar->style()->unpolish(ui->ramProgressBar);
        ui->ramProgressBar->style()->polish(ui->ramProgressBar);

        ui->ramPercent->setProperty("state" , ramState) ;

        ui->ramProgressBar->style()->unpolish(ui->ramProgressBar);
        ui->ramProgressBar->style()->polish(ui->ramProgressBar);
        ui->ramPercent->style()->polish(ui->ramPercent);
        ui->ramPercent->style()->polish(ui->ramPercent);

    }
}

//---------------------------------------------------------------------

void MainWindow::loadProcess()
{
    bool ok ;
    int pid , row ;
    float ram = 0.0f ;
    QString processName ;
    QFile file ;
    float cpuUsage = 0.0f ;

    // Vider le tableau
    ui->processTable->setRowCount(0);

    QDir procDir("/proc");
    QFileInfoList entries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot ) ;

    for(const QFileInfo &entry : as_const(entries))
    {
        pid = entry.fileName().toInt(&ok);

        if( !ok )
        {
            continue ;
        }

        QString pathName = "/proc/" + QString::number(pid) + "/comm" ;
        file.setFileName(pathName);

        if( !file.open(QIODevice::ReadOnly | QIODevice::Text ))
        {
            continue ;
        }

        QTextStream in(&file) ;
        processName = in.readAll().trimmed() ;
        file.close() ;

        QString pathRam = "/proc/" + QString::number(pid) + "/status" ;
        file.setFileName(pathRam);

        if( !file.open(QIODevice::ReadOnly | QIODevice::Text ))
        {
            continue ;
        }

        in.setDevice(&file) ;
        QString contenu = in.readAll() ;
        file.close() ;

        QStringList listLine = contenu.split( "\n" , Qt::SkipEmptyParts ) ;
        for( const QString &line : as_const(listLine) )
        {
            if(line.startsWith("VmRSS:"))
            {
                ram = line.split(" " , Qt::SkipEmptyParts )[1].toFloat() ;
                break;
            }
        }

        ram = ram / 1024.0 ;

        row = ui->processTable->rowCount() ;
        ui->processTable->insertRow(row) ;

        cpuUsage = calculateCpuUsage(pid , m_lastDeltaTotal ) ;

        // Ajout des elements dans le tableau

            // Pour le tri selon ram ou proc ou pid
            NumericTableWidgetItem *ramItem = new NumericTableWidgetItem(QString::number(ram , 'f' , 3 ) + " Mo" ) ;
            ramItem->setData(Qt::UserRole , ram ) ;

            NumericTableWidgetItem *cpuItem = new NumericTableWidgetItem(QString::number(cpuUsage , 'f' , 2) + " %" ) ;
            cpuItem->setData(Qt::UserRole , cpuUsage ) ;

            NumericTableWidgetItem *pidItem = new NumericTableWidgetItem(QString::number(pid)) ;
            pidItem->setData(Qt::UserRole , pid ) ;

            ui->processTable->setItem(row , 0 , new QTableWidgetItem(processName)) ;
            ui->processTable->setItem(row , 1 , pidItem );
            ui->processTable->setItem(row , 2 , ramItem );
            ui->processTable->setItem(row , 3 , cpuItem );
    }
}

float MainWindow::calculateCpuUsage(int pid , long long deltatCpuTotal )
{
    QFile file ;
    QString content ;
    QString path = "/proc/" + QString::number(pid) + "/stat" ;
    QTextStream in ;
    float cpuProcess ;
    long long utime , stime , processTime , deltatProcess = 0 ;

    file.setFileName(path) ;
    if( !file.open(QIODevice::ReadOnly | QIODevice::Text ))
    {
        return 0.0f ;
    }

    in.setDevice(&file);
    content = in.readAll() ;
    file.close() ;

    if(deltatCpuTotal <= 0 )
    {
        return 0.0f;
    }

    utime = content.split( " " , Qt::SkipEmptyParts )[13].toLongLong() ;
    stime = content.split( " " , Qt::SkipEmptyParts )[14].toLongLong() ;

    processTime = utime + stime ;

    deltatProcess = processTime - previousProcessTime.value(pid , 0);

    previousProcessTime[pid] = processTime ;

    cpuProcess = (static_cast<float>(deltatProcess) / static_cast<float>(deltatCpuTotal)) * 100 ;

    return cpuProcess ;
}