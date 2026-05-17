#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow fenetre ;

    QFile file(":/styles/dark.qss");

    if( file.open(QFile::ReadOnly) )
    {
        QString styleSheet = file.readAll();
        app.setStyleSheet(styleSheet) ;
    }
    else
    {
        std::cout << "Erreur style" << std::endl ;
    }

    fenetre.show() ;

    return app.exec() ;
}