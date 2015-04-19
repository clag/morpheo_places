#include "Database.h"
#include <QSqlDatabase>

Database::Database(QString host, QString name, QString user, QString pass)
{
    m_host = host;
    m_name = name;
    m_user = user;
    m_pass = pass;

    m_db = QSqlDatabase::addDatabase("QPSQL");
}

bool Database::connexion(){

    m_db.setHostName(m_host);
    m_db.setDatabaseName(m_name);
    m_db.setUserName(m_user);
    m_db.setPassword(m_pass);

    return m_db.open();
}

Database::~Database(){
    m_db.close();
}

//***************************************************************************************************************************************************
//SUPPRESSION DE TABLE
//
//***************************************************************************************************************************************************

bool Database::dropTable(QString tableName) {
    QSqlQuery testTable;
    testTable.prepare("SELECT * FROM " + tableName + " LIMIT 1;");
    testTable.exec();

    if (! testTable.lastError().isValid()){
        QSqlQueryModel dropTable;
        dropTable.setQuery("DROP TABLE " + tableName + " CASCADE;");

        if (dropTable.lastError().isValid()) {
            emit fatal(QString("drop table %1 : %2").arg(tableName).arg(dropTable.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

    }//endif

    return true;
}

//***************************************************************************************************************************************************
//SUPPRESSION DE COLONNE DANS UNE TABLE
//
//***************************************************************************************************************************************************

bool Database::dropColumn(QString tableName, QString columnName) {
    QSqlQuery testTable;
    testTable.prepare("SELECT * FROM " + tableName + " LIMIT 1;");
    testTable.exec();

    if (! testTable.lastError().isValid()){
        QSqlQueryModel dropColumn;
        dropColumn.setQuery("ALTER TABLE " + tableName + " DROP COLUMN " + columnName + ";");

        if (dropColumn.lastError().isValid()) {
            emit fatal(QString("drop table %1 : %2").arg(tableName).arg(dropColumn.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

    } else {
        emit fatal(QString("drop column %1 : la table %2 censé la contenir n'existe pas").arg(columnName).arg(tableName));
        return false;
    }

    return true;
}

//***************************************************************************************************************************************************
//EXISTENCE DE TABLE
//
//***************************************************************************************************************************************************

bool Database::tableExists(QString tableName) {
    QSqlQuery testTable;
    testTable.prepare("SELECT * FROM " + tableName + " LIMIT 1;");
    return testTable.exec();
}

//***************************************************************************************************************************************************
//EXISTENCE D'UNE COLONNE DANS UNE TABLE
//
//***************************************************************************************************************************************************

bool Database::columnExists(QString tableName, QString columnName) {
    QSqlQuery testTable;
    testTable.prepare("SELECT " + columnName + " FROM " + tableName + " LIMIT 1;");
    return testTable.exec();
}

//***************************************************************************************************************************************************
//ajout d'un attribut combiné par division
//
//***************************************************************************************************************************************************
bool Database::add_att_div(QString table, QString new_att, QString att_1, QString att_2){

    //AJOUT ATTRIBUT DANS VOIES
    QSqlQueryModel addAttribute;
    addAttribute.setQuery(QString("ALTER TABLE %1 ADD %2 %3 DEFAULT 0;").arg(table).arg(new_att).arg("double precision"));

    if (addAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible d'ajouter l'attribut %1 dans la table %2").arg(new_att).arg(table));
        emit fatal(addAttribute.lastError().text());
        return false;
    }

    //CALCUL ET INSERTION DU NOUVEL ATTRIBUT
    QSqlQueryModel updateAttribute;
    updateAttribute.setQuery(QString("UPDATE %1 SET %2=%3/%4 WHERE %4 != 0;").arg(table).arg(new_att).arg(att_1).arg(att_2));

    if (updateAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible de calculer le nouvel attribut %1 dans %2 par division").arg(new_att).arg(table));
        emit fatal(updateAttribute.lastError().text());
        return false;
    }

    return true;

}//end add_att_div

//***************************************************************************************************************************************************
//ajout d'un attribut combiné par multiplication
//
//***************************************************************************************************************************************************

bool Database::add_att_prod(QString table, QString new_att, QString att_1, QString att_2){

    //AJOUT ATTRIBUT DANS VOIES
    QSqlQueryModel addAttribute;
    addAttribute.setQuery(QString("ALTER TABLE %1 ADD %2 %3;").arg(table).arg(new_att).arg("double precision"));

    if (addAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible d'ajouter l'attribut %1 dans la table %2").arg(new_att).arg(table));
        emit fatal(addAttribute.lastError().text());
        return false;
    }

    //CALCUL ET INSERTION DU NOUVEL ATTRIBUT
    QSqlQueryModel updateAttribute;
    updateAttribute.setQuery(QString("UPDATE %1 SET %2=%3*%4;").arg(table).arg(new_att).arg(att_1).arg(att_2));

    if (updateAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible de calculer le nouvel attribut %1 dans %2 par multiplication").arg(new_att).arg(table));
        emit fatal(updateAttribute.lastError().text());
        return false;
    }

    return true;

}//end add_att_prod


//***************************************************************************************************************************************************
//ajout d'un attribut combiné par soustraction
//
//***************************************************************************************************************************************************
bool Database::add_att_dif(QString table, QString new_att, QString att_1, QString att_2){


    if(! columnExists(table, new_att)){
        //AJOUT ATTRIBUT DANS VOIES
        QSqlQueryModel addAttribute;
        addAttribute.setQuery(QString("ALTER TABLE %1 ADD %2 %3 DEFAULT 0;").arg(table).arg(new_att).arg("double precision"));

        if (addAttribute.lastError().isValid()) {
            emit fatal(QString("Impossible d'ajouter l'attribut %1 dans la table %2").arg(new_att).arg(table));
            emit fatal(addAttribute.lastError().text());
            return false;
        }
    }

    //CALCUL ET INSERTION DU NOUVEL ATTRIBUT
    QSqlQueryModel updateAttribute;
    updateAttribute.setQuery(QString("UPDATE %1 SET %2=%3-%4;").arg(table).arg(new_att).arg(att_1).arg(att_2));

    if (updateAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible de calculer le nouvel attribut %1 dans %2 par division").arg(new_att).arg(table));
        emit fatal(updateAttribute.lastError().text());
        return false;
    }

    return true;

}//end add_att_dif

//***************************************************************************************************************************************************
//ajout d'un attribut combiné par soustraction
//
//***************************************************************************************************************************************************
bool Database::add_att_difABS(QString table, QString new_att, QString att_1, QString att_2){

    if(! columnExists(table, new_att)){
        //AJOUT ATTRIBUT DANS VOIES
        QSqlQueryModel addAttribute;
        addAttribute.setQuery(QString("ALTER TABLE %1 ADD %2 %3 DEFAULT 0;").arg(table).arg(new_att).arg("double precision"));

        if (addAttribute.lastError().isValid()) {
            emit fatal(QString("Impossible d'ajouter l'attribut %1 dans la table %2").arg(new_att).arg(table));
            emit fatal(addAttribute.lastError().text());
            return false;
        }
    }

    //CALCUL ET INSERTION DU NOUVEL ATTRIBUT
    QSqlQueryModel updateAttribute;
    updateAttribute.setQuery(QString("UPDATE %1 SET %2=abs(%3-%4);").arg(table).arg(new_att).arg(att_1).arg(att_2));

    if (updateAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible de calculer le nouvel attribut %1 dans %2 par division").arg(new_att).arg(table));
        emit fatal(updateAttribute.lastError().text());
        return false;
    }

    return true;

}//end add_att_difABS


//***************************************************************************************************************************************************
//ajout d'un attribut combiné par addition
//
//***************************************************************************************************************************************************
bool Database::add_att_add(QString table, QString new_att, QString att_1, QString att_2){

    //AJOUT ATTRIBUT DANS VOIES
    QSqlQueryModel addAttribute;
    addAttribute.setQuery(QString("ALTER TABLE %1 ADD %2 %3 DEFAULT 0;").arg(table).arg(new_att).arg("double precision"));

    if (addAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible d'ajouter l'attribut %1 dans la table %2").arg(new_att).arg(table));
        emit fatal(addAttribute.lastError().text());
        return false;
    }

    //CALCUL ET INSERTION DU NOUVEL ATTRIBUT
    QSqlQueryModel updateAttribute;
    updateAttribute.setQuery(QString("UPDATE %1 SET %2=%3+%4;").arg(table).arg(new_att).arg(att_1).arg(att_2));

    if (updateAttribute.lastError().isValid()) {
        emit fatal(QString("Impossible de calculer le nouvel attribut %1 dans %2 par division").arg(new_att).arg(table));
        emit fatal(updateAttribute.lastError().text());
        return false;
    }

    return true;

}//end add_att_add


//***************************************************************************************************************************************************
//ajout d'un attribut de classification
//
//***************************************************************************************************************************************************

bool Database::add_att_cl(QString table, QString new_att, QString att_1, int nb_classes, bool ascendant){

    if (nb_classes == 0) {
        emit fatal("La classification ne peut pas se faire avec 0 classes");
        return false;
    }

    if(! columnExists(table, new_att)){

        //AJOUT ATTRIBUT DANS VOIES
        QSqlQueryModel addAttribute;
        addAttribute.setQuery(QString("ALTER TABLE %1 ADD %2 integer;").arg(table).arg(new_att));

        if (addAttribute.lastError().isValid()) {
            emit fatal(QString("Impossible d'ajouter l'attribut de classification par longueur %1 dans la table %2").arg(new_att).arg(table));
            emit fatal(addAttribute.lastError().text());
            return false;
        }

    }


    //longueur de voie par indice
    QSqlQueryModel queryLengthTot;
    queryLengthTot.setQuery(QString("SELECT SUM(LENGTH) AS LENGTH_TOT FROM %1;").arg(table));

    if (queryLengthTot.lastError().isValid()) {
        emit fatal(QString("Récupération de la somme totale des longueurs dans %1 : %2").arg(table).arg(queryLengthTot.lastError().text()));
        return false;
    }

    float length_tot = queryLengthTot.record(0).value("LENGTH_TOT").toFloat();
    float length_autorisee = length_tot/nb_classes;

    emit information(QString("Classification de %1 dans %2 : length_tot = %3 et length_autorisee = %4").arg(new_att).arg(table).arg(length_tot).arg(length_autorisee));

    QString idname;
    idname = "IDV";

    emit information(QString("ATTENTION : CLASSIFICATION CONFIGUREE POUR LA TABLE VOIE AVEC IDV !!"));

    /*if (table == "VOIES") idname = "IDV";
    else if (table == "AXYZ" || table == "SIF") idname = "IDA";
    //****
    else {
        idname = "GID";
       // emit fatal(QString("La classification n'est pas prise en compte pour la table %1").arg(table));
       // return false;
    }*/
    //****

    //CLASSIFICATION
    QSqlQueryModel classification;
    if (ascendant) classification.setQuery(QString("SELECT %1 AS ID, length AS LENGTH, %2 AS ATT FROM %3 WHERE %2 >= 0 ORDER BY ATT ASC;").arg(idname).arg(att_1).arg(table));
    else classification.setQuery(QString("SELECT %1 AS ID, length AS LENGTH, %2 AS ATT FROM %3 WHERE %2 >= 0 ORDER BY ATT DESC;").arg(idname).arg(att_1).arg(table));

    if (classification.lastError().isValid()) {
        emit fatal(QString("Impossible de recuperer les objets de la table %1, tries par %2").arg(table).arg(att_1));
        emit fatal(classification.lastError().text());
        return false;
    }

    int nbElements = classification.rowCount();

    float current_length_cumul = 0.;
    int current_category = 0;

    float att_ant = -1;
    for(int e = 0; e < nbElements; e++){
        int id = classification.record(e).value("ID").toInt();
        int len = classification.record(e).value("LENGTH").toInt();
        float att = classification.record(e).value("ATT").toFloat();

        if(att_ant != -1){
            //vérification :
            if(ascendant) {
                if(att < att_ant) {
                    emit fatal(QString("La classification n'est pas faite dans l'ordre : att = %1 < att_ant = %2").arg(att).arg(att_ant));
                    return false;
                }
            } else {
                if(att > att_ant) {
                    emit fatal(QString("La classification n'est pas faite dans l'ordre : att = %1 > att_ant = %2").arg(att).arg(att_ant));
                    return false;
                }
            }
        }

        if (current_length_cumul + len >= length_autorisee && current_category != nb_classes - 1) {
            current_category++;
            current_length_cumul = 0.;
        }

        current_length_cumul += len;
        att_ant = att;

        //emit information(QString("id : %1, att : %2, current_category : %3").arg(id).arg(att).arg(current_category));

        //INSERTION EN BASE
        QSqlQueryModel addClassification;
        addClassification.setQuery(QString("UPDATE %1 SET %2 = %3 WHERE %4 = %5 ;").arg(table).arg(new_att).arg(current_category).arg(idname).arg(id));

        if (addClassification.lastError().isValid()) {
            emit fatal(QString("Impossible d'ajouter la valeur de classification %1 dans la table %2 pour l'identifiant %3").arg(new_att).arg(table).arg(id));
            emit fatal(addClassification.lastError().text());
            return false;
        }
    }//end for e

    if (ascendant) {
        emit information(QString("Classification de %1 dans %2 ordre ASC finie.").arg(new_att).arg(table));
    } else {
        emit information(QString("Classification de %1 dans %2 ordre DESC finie.").arg(new_att).arg(table));
    }

    return true;

}//end add_att_cl

