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
#include <iostream>

using namespace std;

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setStyleSheet(R"(
    QWidget {
        background-color: #1e1f26;
        color: white;
        font-size: 14px;
    }

    #cpuCard,
    #gpuCard,
    #ramCard,
    #swapCard {
        background-color: #2b2d39;
        border-radius: 12px;
    }

    QLabel {
        background: transparent;
        padding: 4px;
        color: white ;
    }

    #cpuTitle {
        font-size : 18px;
        font-weight : bold ;
    }

    #cpuPercent {
        font-size : 28px ;
        font-weight : bold ;
        color : #8b5cf6;
    }

    QProgressBar {
        border: none;
        border-radius: 6px;
        background-color: #3b3d4a;
        text-align: center;
        height: 12px;
        padding: 8px;
    }

    QProgressBar::chunk {
        background-color: #8b5cf6;
        border-radius: 6px;
    }

    QTableWidget {
        background-color: #2b2d39;
        border: none;
        border-radius: 12px;
        gridline-color: #3b3d4a;
        padding: 8px;
    }

    QHeaderView::section {
        background-color: #352b52;
        color: white;
        padding: 6px;
        border: none;
        font-weight: bold;
    }

    QTableWidget::item {
        padding: 5px;
    }

    QTableWidget::item:selected {
        background-color: #8b5cf6;
    }
)");
    // Ajout du timer 1s
    QTimer *timer = new QTimer(this) ;

    connect(timer , &QTimer::timeout , this , &MainWindow::majSystemInfo ) ;
    timer->start(1000);

    //Ajustement Automatique des colonnes
    ui->processTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    previousCpuTotalUsage = getCpuUsage() ;


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

    // Si echec de l'ouverture
    if(! file.open(QIODevice::ReadOnly | QIODevice::Text) )
    {
        return 0 ;
    }

    // Lecture de la premier ligne du fichier
    QTextStream in(&file);

    ligne = in.readLine() ;
    liste = ligne.split(" " , Qt::SkipEmptyParts ) ; // decoupage en mot par mot
    file.close() ;

    // Conversion des valeur en long long
    user = liste[1].toLongLong();
    nice = liste[2].toLongLong();
    system = liste[3].toLongLong();
    idle = liste[4].toLongLong();

    // Calcul du totat actuel
    total = user + nice + system + idle ;

    // Calcul des 2 deltats
    deltatTotal = total - m_prevTotal ;
    deltatIdle = idle - m_prevIdle ;


    // Calcule du pourcentage du cpu
    cpuUsage = (1 - ((float)deltatIdle / deltatTotal )) * 100.0 ;

    // Sauvegarde des nouvelles valeurs
    m_prevTotal = total ;
    m_prevIdle = idle ;

    return cpuUsage ;
}

//-------------------------------------------------------------------

float MainWindow::getRamUsage()
{
    QString contenu ;
    QStringList lignes ;
    long long memTotal = 0 , memAvailable = 0 ;
    float memUsage = 0.0f , memPercent ;

    // Ouverture du fichier /pro/meminfo
    QFile file("/proc/meminfo");
    if(! file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(this , "Erreur" , "Impossible de lire le fichier /proc/meminfo .") ;
        return 0.0f;
    }

    // Recuperation de tout le contenu
    contenu = file.readAll() ;

    // decoupage linge par ligne
    lignes = contenu.split("\n");

    // Parcours des lignes et recuperation des valeurs utiles
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

    // Calcul
    if(memTotal == 0)
    {
        return 0.0f;
    }
    memUsage = float(memTotal - memAvailable) ;

    memPercent = memUsage/memTotal * 100 ;

    return (memPercent) ;
}

//-------------------------------------------------------------------
void MainWindow::majSystemInfo()
{
    float cpuUsage ;
    float ramUsage ;

    // Recuperation du charge
    cpuUsage = getCpuUsage() ;
    ramUsage = getRamUsage() ;

    // MAJ des caffichages

    ui->cpuPercent->setText(QString::number(cpuUsage , 'f' , 1 ) + "%") ;
    ui->cpuProgressBar->setValue((int)cpuUsage) ;

    ui->ramPercent->setText(QString::number(ramUsage , 'f' , 1 ) + "%") ;
    ui->ramProgressBar->setValue((int)ramUsage) ;

    loadProcess() ;
}


//---------------------------------------------------------------------

void MainWindow::loadProcess()
{
    bool ok ;
    int pid , row ;
    float ram = 0.0f ;
    QString processName ;
    QFile file ;
    float cpuUsage , currentcpuUsage , deltatTotal ;

    // Vider le tableau
    ui->processTable->setRowCount(0);

    // Ouverture du dossier
    QDir procDir("/proc");
    QFileInfoList entries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot ) ;


    // Calcul Cpu total system avant la boucle
    currentcpuUsage = getCpuUsage() ;
    deltatTotal = currentcpuUsage - previousCpuTotalUsage ;

    //Parcours du dossier
    for(const QFileInfo &entry : as_const(entries))
    {
        pid = entry.fileName().toInt(&ok);

        if( !ok ) // Si c'est pas un nombre
        {
            continue ;
        }

        // Constructioon du chemin vers comm
        QString pathName = "/proc/" + QString::number(pid) + "/comm" ;

        // ouverture du path et recuperation du
        file.setFileName(pathName);

        if( !file.open(QIODevice::ReadOnly | QIODevice::Text ))
        {
            continue ;
        }

        // Recuperation des infos
        QTextStream in(&file) ;
        processName = in.readAll().trimmed() ;
        file.close() ;

        // Construction du chemin vers status
        QString pathRam = "/proc/" + QString::number(pid) + "/status" ;

        file.setFileName(pathRam); // Ouverture du fichier
        if( !file.open(QIODevice::ReadOnly | QIODevice::Text ))
        {
            continue ;
        }

        // Recuperation du Ram utilisé
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
        // Conversion du ram en Mb
        ram = ram / 1024.0 ;

        // ajout d'une ligne au tableau
        row = ui->processTable->rowCount() ;
        ui->processTable->insertRow(row) ;

        cpuUsage = calculateCpuUsage(pid , deltatTotal ) ;
        // Ajout du PID et du NOM et du RAM
        ui->processTable->setItem(row , 0 , new QTableWidgetItem(processName)) ;
        ui->processTable->setItem(row , 1 , new QTableWidgetItem(QString::number(pid)));
        ui->processTable->setItem(row , 2 , new QTableWidgetItem(QString::number(ram , 'f' , 3 ) + " Mo" ));
        ui->processTable->setItem(row , 3 , new QTableWidgetItem(QString::number(cpuUsage , 'f' , 2) + " %" ))  ;
    }

    //Maj du previousCpuUsage
    previousCpuTotalUsage = currentcpuUsage ;

}

float MainWindow::calculateCpuUsage(int pid , float deltatCpuTotal )
{
    QFile file ;
    QString content ;
    QString path = "/proc/" + QString::number(pid) + "/stat" ;
    QTextStream in ;
    float cpuProcess ;
    long long utime ,stime , processTime , deltatProcess = 0 ;

    // Ouverture du fichier
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

    // Recuperation des donnné utiles ( utime 13 et stime 14 )
    utime = content.split( " " , Qt::SkipEmptyParts )[13].toLongLong() ;
    stime = content.split( " " , Qt::SkipEmptyParts )[14].toLongLong() ;

    // Calcule du process time
    processTime = utime + stime ;

    // Calcul du deltatProcess
    deltatProcess = processTime - previousProcessTime.value(pid , 0);

    // Sauvegarde du nouvell valeur
    previousProcessTime[pid] = processTime ;

    cout << deltatProcess << endl ;

    // Calcul cpuProcess
    cpuProcess = (static_cast<float>(deltatProcess) / deltatCpuTotal ) * 100 ;

    return cpuProcess ;
}