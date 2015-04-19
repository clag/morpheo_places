#include "Arcs.h"

#include <iostream>
using namespace std;

Arcs::Arcs(Database* db, Logger* log, Graphe* graphe, Voies* voies, WayMethods::methodID methode, double seuil)
{
    pDatabase = db;
    pLogger = log;

    pLogger->INFO(QString("Methode de contruction des voies : %1").arg(WayMethods::MethodeVoies_name[methode]));

    m_Graphe = graphe;
    m_Voies = voies;
    m_seuil_angle = seuil;
    m_methode = methode;
}

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE ARCS EN BDD
//
//***************************************************************************************************************************************************

bool Arcs::build_ARCS(){

    //SUPPRESSION DE LA TABLE ARCS SI DEJA EXISTANTE
    if (! pDatabase->tableExists("ARCS")) {

        pLogger->INFO("-------------------------- build_ARCS START ------------------------");

        //CREATION DE LA TABLE ARCS

        QString column_nom;
        if (pDatabase->columnExists("brut_arcs", "nom_voie_g")){column_nom =  "nom_voie_g";}
        else if (pDatabase->columnExists("brut_arcs", "nom_rue_g")){column_nom =  "nom_rue_g";}
        else {
            pLogger->ERREUR(QString("Impossible de trouver la colonne de noms."));
            return false;
        }

        QSqlQueryModel createTableARCS;
        createTableARCS.setQuery(QString("CREATE TABLE ARCS AS SELECT GID AS IDA, %1 AS NOM, GEOM FROM brut_arcs;").arg(column_nom));

        if (createTableARCS.lastError().isValid()) {
            pLogger->ERREUR(QString("createTableARCS : %1").arg(createTableARCS.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        //AJOUT DE L'ATTRIBUT IDV

        QSqlQueryModel addIdvInARCS;
        addIdvInARCS.setQuery("ALTER TABLE ARCS ADD IDV integer;");

        if (addIdvInARCS.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter l'attribut idv dans ARCS : %1").arg(addIdvInARCS.lastError().text()));
            return false;
        }

        //RECHERCHE DE LA VOIE CORRESPONDANT A L'ARC

        QSqlQueryModel *geomFromARCS = new QSqlQueryModel();

        geomFromARCS->setQuery("SELECT IDA, GEOM FROM ARCS;");

        if (geomFromARCS->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer la géométries dans ARCS : %1").arg(geomFromARCS->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //POUR CHAQUE ARC, AJOUT DE LA VOIE

        for(int a = 0; a < geomFromARCS->rowCount(); a++){
            int ida = geomFromARCS->record(a).value("IDA").toInt();
            QString geom_a = geomFromARCS->record(a).value("GEOM").toString();
            int idv;

            QSqlQueryModel *idvFromVOIES = new QSqlQueryModel();

            idvFromVOIES->setQuery(QString("SELECT IDV FROM VOIES WHERE ST_Covers(MULTIGEOM,'%1');").arg(geom_a));

            if (idvFromVOIES->lastError().isValid()) {
                pLogger->ERREUR(QString("Impossible de recuperer l'idv dans VOIES : %1").arg(idvFromVOIES->lastError().text()));
                return false;
            }//end if : test requete QSqlQueryModel

            if(idvFromVOIES->rowCount() > 1) {
                pLogger->ERREUR(QString("Plus d'un retour d'idv pour un arc (!)"));
                for(int i=0; i<idvFromVOIES->rowCount(); i++){
                    pLogger->INFO(QString("-- idv = %1 pour l'arc' %2 --").arg(idvFromVOIES->record(i).value("IDV").toInt()).arg(a));
                }

                return false;
            }

            idv = idvFromVOIES->record(0).value("IDV").toInt();

            delete idvFromVOIES;

            //INSERTION EN BASE
            QSqlQuery addIdvAttInARCS;
            addIdvAttInARCS.prepare("UPDATE ARCS SET IDV = :IDV WHERE ida = :IDA ;");
            addIdvAttInARCS.bindValue(":IDA",ida );
            addIdvAttInARCS.bindValue(":IDV",idv);

            if (! addIdvAttInARCS.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'idv' %1 pour l'arc' %2").arg(idv).arg(ida));
                pLogger->ERREUR(addIdvAttInARCS.lastError().text());
                return false;
            }

        }//end for a

        delete geomFromARCS;

        pLogger->INFO("--------------------------- build_ARCS END --------------------------");
    } else {
        pLogger->INFO("------------------------- ARCS already exists -----------------------");
    }

    return true;

}//END build_ARCS

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE RUES EN BDD
//
//***************************************************************************************************************************************************

bool Arcs::build_RUES(){

    //SUPPRESSION DE LA TABLE RUES SI DEJA EXISTANTE
    if (! pDatabase->tableExists("RUES")) {

        pLogger->INFO("-------------------------- build_RUES START ------------------------");

        //CREATION DE LA TABLE RUES

        QSqlQueryModel createTableRUES;
        createTableRUES.setQuery("CREATE TABLE RUES (IDR SERIAL NOT NULL PRIMARY KEY, NOM VARCHAR(140), MULTIGEOM geometry);");

        if (createTableRUES.lastError().isValid()) {
            pLogger->ERREUR(QString("createTableRUES : %1").arg(createTableRUES.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        // RECHERCHE DU NOMBRE DE RUES

        QSqlQueryModel countRues;
        countRues.setQuery("SELECT COUNT(IDA) AS COMPTE FROM (SELECT DISTINCT(NOM), IDA FROM ARCS WHERE NOM != '')  AS NOMS;");

        if (countRues.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de compter les rues : %1").arg(countRues.lastError().text()));
            return false;
        }

        m_nbRues = countRues.record(0).value("COMPTE").toInt();

         pLogger->INFO(QString("m_nbRues : %1").arg(m_nbRues));


        //RECHERCHE DES NOMS CORRESPONDANT AUX ARCS

        QSqlQueryModel *nomFromARCS = new QSqlQueryModel();

        nomFromARCS->setQuery("SELECT DISTINCT(NOM) FROM ARCS WHERE NOM != '';");

        if (nomFromARCS->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer le nom dans ARCS : %1").arg(nomFromARCS->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DE GEOMETRIES POUR CHAQUE RUES

        pLogger->INFO(QString("nomFromARCS->rowCount() : %1").arg(nomFromARCS->rowCount()));

        for(int n = 0; n < nomFromARCS->rowCount(); n++){

            QString nom = nomFromARCS->record(n).value("NOM").toString();
            nom.remove("'");

            QSqlQueryModel *geomFromARCS = new QSqlQueryModel();

            geomFromARCS->setQuery(QString("SELECT GEOM FROM ARCS WHERE NOM = '%1';").arg(nom));

            //pLogger->INFO(QString("requête envoyée : %1").arg(geomFromARCS->query().lastQuery()));

            if (geomFromARCS->lastError().isValid()) {
                pLogger->ERREUR(QString("Impossible de recuperer la géométries dans ARCS : %1").arg(geomFromARCS->lastError().text()));
                return false;
            }//end if : test requete QSqlQueryModel

            QString geom_arcs[geomFromARCS->rowCount()];

            for(int a = 0; a < geomFromARCS->rowCount(); a++){
                geom_arcs[a] = geomFromARCS->record(a).value("GEOM").toString();
            }//end for a

            QString geom1;

            if(geomFromARCS->rowCount() != 0){

                geom1 = geom_arcs[0];
                QString geom2;

                for(int a = 1; a < geomFromARCS->rowCount(); a++){

                    geom2 = geom_arcs[a];

                    // ASSOCIATION DES GEOMETRIES

                    QSqlQueryModel geomUnion;
                    geomUnion.setQuery(QString("SELECT ST_UNION(ST_AsText('%1'), ST_AsText('%2')) AS GEOM3;").arg(geom1).arg(geom2));

                    if (geomUnion.lastError().isValid()) {
                        pLogger->ERREUR(QString("Impossible de réunir les géométries : %1").arg(geomUnion.lastError().text()));
                        return false;
                    }

                    geom1 = geomUnion.record(0).value("GEOM3").toString();

                }//end for a

                delete geomFromARCS;

                QString addRue = QString("INSERT INTO RUES (IDR, NOM, MULTIGEOM) VALUES (%1 , '%2', '%3');").arg(n).arg(nom).arg(geom1);
                QSqlQuery addInRUES;
                addInRUES.prepare(addRue);

                if (! addInRUES.exec()) {
                    pLogger->ERREUR(QString("Impossible d'inserer la geometrie %1 pour la rue %2").arg(geom1).arg(nom));
                    pLogger->ERREUR(addInRUES.lastError().text());
                    return false;
                }

            }//endif
            else{pLogger->ATTENTION(QString("geometrie vide pour le nom : %1 (n: %2)").arg(nom).arg(n));}

        }//end for n

        delete nomFromARCS;




        pLogger->INFO("--------------------------- build_RUES END --------------------------");
    } else {
        pLogger->INFO("------------------------- RUES already exists -----------------------");
    }

    return true;

}//END build_ARCS

//***************************************************************************************************************************************************
//CONSTRUCTION DES TABLES
//
//***************************************************************************************************************************************************

bool Arcs::do_Arcs(){

    // Construction de la table ARCS en BDD
    if (! build_ARCS()) {
        if (! pDatabase->dropTable("ARCS")) {
            pLogger->ERREUR("build_ARCS en erreur, ROLLBACK (drop ARCS) echoue");
        } else {
            pLogger->INFO("build_ARCS en erreur, ROLLBACK (drop ARCS) reussi");
        }
        return false;
    }

    // Construction de la table RUES en BDD
    if (! build_RUES()) {
        if (! pDatabase->dropTable("RUES")) {
            pLogger->ERREUR("build_RUES en erreur, ROLLBACK (drop RUES) echoue");
        } else {
            pLogger->INFO("build_RUES en erreur, ROLLBACK (drop RUES) reussi");
        }
        return false;
    }

    return true;

}//end do_Voie
