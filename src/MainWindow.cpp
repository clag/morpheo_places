#include "MainWindow.h"
#include "ui_mainwindow.h"
#include "Voies.h"
#include "Arcs.h"
#include "ui_mainwindow.h"
#include <QFileDialog>

MainWindow::MainWindow(Logger* log, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    pLogger = log;

    ui->statusBar->showMessage("Ready to be used");

    // Récupération des valeur possibles pour le calcul des voies depuis l'énumération
    for (int i = 0; i < WayMethods::numMethods; i++) {
        ui->methodComboBox->addItem(WayMethods::MethodeVoies_name[i]);
    }
    ui->methodComboBox->setCurrentIndex(WayMethods::ANGLE_MIN); // valeur par défaut

    // Connexion des messages émis depuis le logger ou la database
    connect(pLogger,SIGNAL(information(QString)),this,SLOT(logInformation(QString)));
    connect(pLogger,SIGNAL(debug(QString)),this,SLOT(logDebug(QString)));
    connect(pLogger,SIGNAL(warning(QString)),this,SLOT(logWarning(QString)));
    connect(pLogger,SIGNAL(fatal(QString)),this,SLOT(logFatal(QString)));

    connect(ui->calculatePushButton,SIGNAL(clicked()),this,SLOT(calculate()));
    connect(ui->modifyPushButton,SIGNAL(clicked()),this,SLOT(modify()));

    connect(ui->browsePushButton, SIGNAL(clicked()), this, SLOT(browse()));

    connect(ui->classificationRadioButton, SIGNAL(toggled(bool)), this, SLOT(optionsModification(bool)));

}

MainWindow::~MainWindow()
{
    delete ui;
    delete pLogger;
}

/***************** Fonction principale ******************/

void MainWindow::calculate() {

    // on vide le logger d'eventuels anciens calculs
    ui->loggerTextBrowser->clear();
    ui->statusBar->showMessage("");

    /******* logger *******/
    pLogger->setDebugActive(ui->debugCheckBox->isChecked());
    QApplication::processEvents();

    /****** database ******/

    QString host = ui->dbhostLineEdit->text();
    QString name = ui->dbnameLineEdit->text();
    QString user = ui->dbuserLineEdit->text();
    QString pass = ui->dbpassswordLineEdit->text();

    pDatabase = new Database(host, name, user, pass);

    ui->statusBar->showMessage("Connexion");
    pLogger->INFO("Trying to connect you");
    if (! pDatabase->connexion()) {
        mettreEnErreur("Erreur Connexion");
        return;
    }
    pLogger->INFO("Connected !");

    // On connecte les logger de la database, pour afficher les messages dans la boite de dialogue
    connect(pDatabase,SIGNAL(information(QString)),this,SLOT(logInformation(QString)));
    connect(pDatabase,SIGNAL(debug(QString)),this,SLOT(logDebug(QString)));
    connect(pDatabase,SIGNAL(warning(QString)),this,SLOT(logWarning(QString)));
    connect(pDatabase,SIGNAL(fatal(QString)),this,SLOT(logFatal(QString)));


    QDateTime start = QDateTime::currentDateTime();
    pLogger->INFO(QString("Started : %1").arg(start.toString()));

    //------------------------------- Définition des paramètres de l'étude

    //+++
    WayMethods::methodID methode = (WayMethods::methodID) ui->methodComboBox->currentIndex();
    double seuil_angle = ui->thresholdDoubleSpinBox->value();
    //+++

 //   for(seuil_angle = 148.; seuil_angle<=180; seuil_angle+=2){

    if (ui->dropTABLESCheckBox->isChecked()){
        pDatabase->dropTable("SXYZ");
        pDatabase->dropTable("SIF");
        pDatabase->dropTable("ANGLES");
        pDatabase->dropTable("INFO");
        pDatabase->dropTable("VOIES");
        pLogger->INFO("table VOIES has been droped");
    }

        //------------------------------- Création du graphe
        Graphe *graphe_courant = new Graphe(pDatabase, pLogger, ui->BufferDoubleSpinBox->value());

        ui->statusBar->showMessage("Graph in progress");
        QApplication::processEvents();
        if (! graphe_courant->do_Graphe(ui->arcstablenameLineEdit->text())) {
            mettreEnErreur("Cannot calculate graph");
            return;
        }

        //------------------------------- Calculs sur les voies
        Voies *voies_courantes = new Voies(pDatabase, pLogger, graphe_courant, methode, seuil_angle, ui->arcstablenameLineEdit->text(), ui->directoryLineEdit->text());
        pLogger->INFO("voies creees");


        ui->statusBar->showMessage("Ways in progress");
        QApplication::processEvents();
        if (! voies_courantes->do_Voies()) {
            mettreEnErreur("Cannot calculate ways");
            return;
        }

        ui->statusBar->showMessage("Ways' attributes in progress");
        QApplication::processEvents();
        if (! voies_courantes->do_Att_Voie(ui->connexionCheckBox->isChecked(), ui->useCheckBox->isChecked(), ui->inclusionCheckBox->isChecked(), ui->gradientCheckBox->isChecked(), ui->localAccesscheckBox->isChecked())) {
            mettreEnErreur("Cannot calculate ways' attributes");
            return;
        }

        ui->statusBar->showMessage("Edges' attributes in progress");
        QApplication::processEvents();
        if (! voies_courantes->do_Att_Arc()) {
            mettreEnErreur("Cannot calculate edges' attributes");
            return;
        }


        //------------------------------- Calculs sur les arcs
        Arcs *arcs_courants = new Arcs(pDatabase, pLogger, graphe_courant, voies_courantes, methode, seuil_angle);
        pLogger->INFO("arcs creees");

        ui->statusBar->showMessage("Arcs in progress");
        QApplication::processEvents();
        if (ui->arcRueCheckBox->isChecked() && ! arcs_courants->do_Arcs()) {
            mettreEnErreur("Cannot calculate arcs");
            return;
        }

 //   }//end for seuil

    ui->statusBar->showMessage("It's all right");
    QApplication::processEvents();

    QDateTime end = QDateTime::currentDateTime();
    pLogger->INFO(QString("End : %1").arg(end.toString()));
    pLogger->INFO(QString("Temps total d'execution' : %1 minutes").arg(start.secsTo(end) / 60.));
}

void MainWindow::modify() {

    // on vide le logger d'eventuels anciens calculs
    ui->loggerTextBrowser->clear();
    ui->statusBar->showMessage("");

    /******* logger *******/
    pLogger->setDebugActive(ui->debugCheckBox->isChecked());
    QApplication::processEvents();

    /****** database ******/

    QString host = ui->dbhostLineEdit->text();
    QString name = ui->dbnameLineEdit->text();
    QString user = ui->dbuserLineEdit->text();
    QString pass = ui->dbpassswordLineEdit->text();

    pDatabase = new Database(host, name, user, pass);

    ui->statusBar->showMessage("Connexion");
    pLogger->INFO("Trying to connect you");
    if (! pDatabase->connexion()) {
        mettreEnErreur("Erreur Connexion");
        return;
    }
    pLogger->INFO("Connected !");

    // On connecte les logger de la database, pour afficher les messages dans la boite de dialogue
    connect(pDatabase,SIGNAL(information(QString)),this,SLOT(logInformation(QString)));
    connect(pDatabase,SIGNAL(debug(QString)),this,SLOT(logDebug(QString)));
    connect(pDatabase,SIGNAL(warning(QString)),this,SLOT(logWarning(QString)));
    connect(pDatabase,SIGNAL(fatal(QString)),this,SLOT(logFatal(QString)));


    QDateTime start = QDateTime::currentDateTime();
    pLogger->INFO(QString("Started : %1").arg(start.toString()));

    QString table = ui->arcstablenameLineEdit->text();
    QString att_1 = ui->inputatt1LineEdit->text();
    QString new_att1 = ui->resultattLineEdit->text();

    if (ui->classificationRadioButton->isChecked()) {
        int nb_classes = ui->classnbSpinBox->value();
        bool ascendant = ! ui->descentCheckBox->isChecked();
        if (! pDatabase->add_att_cl(table, new_att1, att_1, nb_classes, ascendant)) {
            mettreEnErreur("Erreur classification");
            return;
        }

    } else if (ui->additionRadioButton->isChecked()) {
        QString att_2 = ui->inputatt2LineEdit->text();
        if (! pDatabase->add_att_add(table, new_att1, att_1, att_2)) {
            mettreEnErreur("Erreur addition");
            return;
        }

    } else if (ui->soustractionRadioButton->isChecked()) {
        QString att_2 = ui->inputatt2LineEdit->text();
        if (! pDatabase->add_att_dif(table, new_att1, att_1, att_2)) {
            mettreEnErreur("Erreur soustraction");
            return;
        }

    } else if (ui->multiplicationRadioButton->isChecked()) {
        QString att_2 = ui->inputatt2LineEdit->text();
        if (! pDatabase->add_att_prod(table, new_att1, att_1, att_2)) {
            mettreEnErreur("Erreur multiplication");
            return;
        }

    } else if (ui->divisionRadioButton->isChecked()) {
        QString att_2 = ui->inputatt2LineEdit->text();
        if (! pDatabase->add_att_div(table, new_att1, att_1, att_2)) {
            mettreEnErreur("Erreur division");
            return;
        }

    } else if (ui->absoluteDiffRadioButton->isChecked()) {
        QString att_2 = ui->inputatt2LineEdit->text();
        if (! pDatabase->add_att_difABS(table, new_att1, att_1, att_2)) {
            mettreEnErreur("Erreur différence absolue");
            return;
        }

    } else {
        pLogger->ERREUR("Action have to be precised");
        mettreEnErreur("Nothing to do");
        return;
    }

    ui->statusBar->showMessage("It's all right");
    QApplication::processEvents();

    QDateTime end = QDateTime::currentDateTime();
    pLogger->INFO(QString("End : %1").arg(end.toString()));
    pLogger->INFO(QString("Temps total d'execution' : %1 minutes").arg(start.secsTo(end) / 60.));

}

void MainWindow::browse()
{
    QString directory = QFileDialog::getExistingDirectory(this, tr("Chose a directory..."), QDir::currentPath());

    if (! directory.isEmpty()) {
        ui->directoryLineEdit->setText(directory);
    }
}


/********************* Slots **************************/

void MainWindow::logDebug( QString qsDebug )
{
    ui->loggerTextBrowser->append(QString("<span style=\"color:black\">%1</span>").arg(qsDebug));
    QApplication::processEvents();

}//logDebug

void MainWindow::logInformation( QString qsInfo )
{
    ui->loggerTextBrowser->append(QString("<span style=\"color:blue\">%1</span>").arg(qsInfo));
    QApplication::processEvents();

}//logInformation

void MainWindow::logWarning( QString qsWarning )
{
    ui->loggerTextBrowser->append(QString("<span style=\"color:orange\">%1</span>").arg(qsWarning));
    QApplication::processEvents();

}//logWarning

void MainWindow::logFatal( QString qsFatalError )
{
    ui->loggerTextBrowser->append(QString("<span style=\"color:red\">%1</span>").arg(qsFatalError));
    QApplication::processEvents();

}//logFatal

void MainWindow::optionsModification(bool checked) {
    if (checked) {
        ui->classnbLabel->setEnabled(true);
        ui->classnbSpinBox->setEnabled(true);

        ui->orderLabel->setEnabled(true);
        ui->descentCheckBox->setEnabled(true);

        ui->inputatt2Label->setEnabled(false);
        ui->inputatt2LineEdit->setEnabled(false);


    } else {
        ui->classnbLabel->setEnabled(false);
        ui->classnbSpinBox->setEnabled(false);

        ui->orderLabel->setEnabled(false);
        ui->descentCheckBox->setEnabled(false);

        ui->inputatt2Label->setEnabled(true);
        ui->inputatt2LineEdit->setEnabled(true);

    }

}


