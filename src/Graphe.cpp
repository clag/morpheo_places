#include "Graphe.h"

#include <QtSql>
#include <QString>

#include <iostream>
using namespace std;

#include <stdlib.h>
#include <math.h>
#include <sstream>

#include <vector>


//***************************************************************************************************************************************************
//RECHERCHE DE L'IDENTIFIANT D'UN SOMMET A PARTIR DE SES COORDONNEES / AJOUTS DANS LA TABLE DES IMPASSES SI NON TROUVE
//
//***************************************************************************************************************************************************

/** \brief Retourne l'identifiant de la place correpondant au point fourni, -1 en cas d'erreur.
 *  \abstract Si la place n'existe pas en base, c'est une erreur
 */
int Graphe::find_idp(QVariant point){

    QSqlQuery  req_places;
    req_places.prepare("SELECT ID, DEGRE FROM PLACES WHERE st_dwithin(:POINT, GEOM, 0.001)");
    req_places.bindValue(":POINT", point);

    if (! req_places.exec()) {
        pLogger->ERREUR(QString("req_places : %1").arg(req_places.lastError().text()));
        return -1;
    }

    QSqlQueryModel req_places_model;
    req_places_model.setQuery(req_places);

    if(req_places_model.rowCount() > 1){
        pLogger->ERREUR(QString("req_places : Pas normal d'avoir plusieurs places pour le bout de l'arc"));
        return -1;
    } else if (req_places_model.rowCount() == 1 ) {
        QSqlQuery updateDeg;
        updateDeg.prepare("UPDATE PLACES SET DEGRE=:DEG WHERE ID=:IDP");
        updateDeg.bindValue(":IDP", req_places_model.record(0).value("ID").toInt());
        updateDeg.bindValue(":DEG", req_places_model.record(0).value("DEGRE").toInt() + 1);

        if (! updateDeg.exec()) {
            pLogger->ERREUR(QString("updateDeg : %1").arg(updateDeg.lastError().text()));
            return -1;
        }

        return req_places_model.record(0).value("ID").toInt();
    } else {
        pLogger->ERREUR(QString("point : %1").arg(point.toString()));
        pLogger->ERREUR(QString("req_places : Pas normal de n'avoir auncune place au bout de l'arc"));
        return -1;
    }

}// END find_ids

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE PIF
//
//***************************************************************************************************************************************************

bool Graphe::build_PIF(QSqlQueryModel* arcs_bruts){


    //PRESENCE DE LA TABLE PIF ? SI DEJA EXISTANTE, rien à faire

    if (! pDatabase->tableExists("PIF")) {

        // On remet les degrés dans la table PLACES

        QSqlQueryModel initPlaces;
        initPlaces.setQuery("UPDATE PLACES SET DEGRE = 0;");

        if (initPlaces.lastError().isValid()) {
            pLogger->ERREUR(QString("initPlaces : %1").arg(initPlaces.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel


        pLogger->INFO("--------------------------- build_PIF START --------------------------");

        //CREATION DE LA TABLE SIF D'ACCUEIL DEFINITIVE

        QSqlQueryModel createTablePIF;
        createTablePIF.setQuery("CREATE TABLE PIF ( IDA SERIAL NOT NULL PRIMARY KEY, PI bigint, PF bigint, GEOM geometry, IDV integer, STRUCT float, AZIMUTH_I double precision, AZIMUTH_F double precision, "
                                "FOREIGN KEY (PI) REFERENCES PLACES(ID), "
                                "FOREIGN KEY (PF) REFERENCES PLACES(ID) );");

        if (createTablePIF.lastError().isValid()) {
            pLogger->ERREUR(QString("createTablePIF : %1").arg(createTablePIF.lastError().text()));
            return false;
        }//end if : test requête QSqlQueryModel

        //EXTRACTION DES DONNEES
        for(int i=0; i<m_nbArcs; i++){

            int ida = arcs_bruts->record(i).value("IDA").toInt();
            QVariant point_si = arcs_bruts->record(i).value("POINT_SI");
            QVariant point_sf = arcs_bruts->record(i).value("POINT_SF");
            QVariant geom = arcs_bruts->record(i).value("GEOM");

            //IDENTIFICATION DES SOMMETS SI ET SF DANS LA TABLE DES SOMMETS
            int id_pi = this->find_idp(point_si);
            int id_pf = this->find_idp(point_sf);
            if (id_pi < 0 || id_pf < 0) {
                pLogger->ERREUR("Erreur dans l'identification des places initiale et finale");
                return false;
            }

            //SAUVEGARDE EN BASE
            QSqlQuery addInPIF;
            addInPIF.prepare("INSERT INTO PIF(IDA, PI, PF, AZIMUTH_I, AZIMUTH_F, GEOM) VALUES (:IDA, :PI, :PF, :AI, :AF, :GEOM);");
            addInPIF.bindValue(":IDA",QVariant(ida));
            addInPIF.bindValue(":SI",QVariant(id_pi));
            addInPIF.bindValue(":SF",QVariant(id_pf));
            addInPIF.bindValue(":GEOM",QVariant(geom));
            addInPIF.bindValue(":AI",arcs_bruts->record(i).value("AZIMUTH_I").toDouble() );
            addInPIF.bindValue(":AF",arcs_bruts->record(i).value("AZIMUTH_F").toDouble() );

            if (! addInPIF.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'identifiant des places %1 et %2 dans PIF. Arc : %3 / record %4").arg(id_pi).arg(id_pf).arg(ida).arg(i));
                pLogger->ERREUR(addInPIF.lastError().text());
                return false;
            }

        }//endfor

        pLogger->INFO("--------------------------- build_PIF END ----------------------------");
    } else {
        pLogger->INFO("-------------------------- PIF already exists ------------------------");
    }



    QSqlQueryModel nbImpasses;
    nbImpasses.setQuery("SELECT COUNT(*) AS nb FROM PLACES WHERE DEGRE=1; ");

    if (nbImpasses.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre d'impasses : %1").arg(nbImpasses.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    m_nbImpasses = nbImpasses.record(0).value("nb").toInt();



    QSqlQueryModel nbIntersections;
    nbIntersections.setQuery("SELECT COUNT(*) AS nb FROM PLACES WHERE DEGRE>1; ");

    if (nbIntersections.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre d'intersections : %1").arg(nbIntersections.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    m_nbIntersections = nbIntersections.record(0).value("nb").toInt();

    QSqlQueryModel nbPlaces;
    nbPlaces.setQuery("SELECT COUNT(*) AS nb FROM PLACES; ");

    if (nbPlaces.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre de places : %1").arg(nbPlaces.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    m_nbPlaces = nbPlaces.record(0).value("nb").toInt();



    QSqlQueryModel nbPlacesNulles;
    nbPlacesNulles.setQuery("SELECT COUNT(*) AS nb FROM PLACES WHERE DEGRE=0; ");

    if (nbPlacesNulles.lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de récupérer le nombre de places isolées : %1").arg(nbPlacesNulles.lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    if (nbPlacesNulles.record(0).value("nb").toInt() != 0) {
        pLogger->ATTENTION("Il ne devrait pas y avoir de places isolées (degré nul)");
        return false;
    }



    pLogger->INFO(QString("Nombre total de places : %1, dont %2 intersections et %3 impasses").arg(m_nbPlaces).arg(m_nbIntersections).arg(m_nbImpasses));

    if (m_nbPlaces != m_nbImpasses + m_nbIntersections) {
        pLogger->ERREUR("Nombre de sommets, impasses et intersections incohérent : ça cache un mal-être");
        return false;
    }

    return true;
}

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE PANGLES
//
//***************************************************************************************************************************************************

bool Graphe::build_PANGLES(){


    if (! pDatabase->tableExists("PANGLES")) {

        pLogger->INFO("-------------------------- build_PANGLES START ------------------------");

        QSqlQueryModel createTablePANGLES;
        createTablePANGLES.setQuery("CREATE TABLE PANGLES (ID SERIAL NOT NULL PRIMARY KEY, IDP bigint, IDA1 bigint, IDA2 bigint, ANGLE double precision, COEF double precision, USED boolean, "
                                "FOREIGN KEY (IDP) REFERENCES PLACES(ID), FOREIGN KEY (IDA1) REFERENCES PIF(IDA), FOREIGN KEY (IDA2) REFERENCES PIF(IDA) );");

        if (createTablePANGLES.lastError().isValid()) {
            pLogger->ERREUR(QString("createTablePANGLES : %1").arg(createTablePANGLES.lastError().text()));
            return false;
        }

        QSqlQueryModel* modelArcs = new QSqlQueryModel();
        QSqlQuery queryArcs;
        queryArcs.prepare("SELECT IDA AS IDA, ST_StartPoint(GEOM) AS SI, ST_EndPoint(GEOM) AS SF, PI AS PI, PF AS PF, AZIMUTH_I AS AI, AZIMUTH_F AS AF FROM PIF WHERE PI = :IDPI OR PF = :IDPF;");


        QSqlQuery addAngle;
        addAngle.prepare("INSERT INTO PANGLES(IDP, IDA1, IDA2, ANGLE, COEF, USED) VALUES (:IDP, :IDA1, :IDA2, :ANGLE, :COEF, FALSE);");

        QSqlQuery calcAzimuth;
        calcAzimuth.prepare("SELECT ST_azimuth(:S11::geometry, :S21::geometry) AS AZ, ST_azimuth(:S22::geometry, :S12::geometry) AS AZinv, ST_distance(:S13,:S23) AS DIST;");

        for (int p = 1; p <= m_nbPlaces; p++) {
            queryArcs.bindValue(":IDPI",p);
            queryArcs.bindValue(":IDPF",p);

            if (! queryArcs.exec()) {
                pLogger->ERREUR(QString("Récupération arcs dans build_PANGLES pour la place %1 : %2").arg(p).arg(queryArcs.lastError().text()));
                return false;
            }

            modelArcs->setQuery(queryArcs);

            for (int a1 = 0; a1 < modelArcs->rowCount(); a1++) {
                int ida1 = modelArcs->record(a1).value("IDA").toInt();
                int pi1 = modelArcs->record(a1).value("PI").toInt();
                int pf1 = modelArcs->record(a1).value("PF").toInt();
                QVariant si1 = modelArcs->record(a1).value("SI");
                QVariant sf1 = modelArcs->record(a1).value("SF");
                double azi1 = modelArcs->record(a1).value("AI").toDouble();
                double azf1 = modelArcs->record(a1).value("AF").toDouble();



                // On traite le cas particulier de l'arc boucle (pi = pf)
                if (pi1 == pf1 && pi1 == p) {
                    double angle = getAngleFromAzimuths(azi1, azf1);

                    calcAzimuth.bindValue(":S11", si1);
                    calcAzimuth.bindValue(":S21", sf1);
                    calcAzimuth.bindValue(":S12", si1);
                    calcAzimuth.bindValue(":S22", sf1);
                    calcAzimuth.bindValue(":S13", si1);
                    calcAzimuth.bindValue(":S23", sf1);

                    if (! calcAzimuth.exec()) {
                        pLogger->ERREUR(QString("Récupération de l'azimuth et de la distance : %1").arg(calcAzimuth.lastError().text()));
                        return false;
                    }

                    QSqlQueryModel resAzDist;
                    resAzDist.setQuery(calcAzimuth);
                    double az = resAzDist.record(0).value("AZ").toDouble();
                    double azinv = resAzDist.record(0).value("AZinv").toDouble();
                    double dist = resAzDist.record(0).value("AZ").toDouble();

                    double phi1 = getAngleFromAzimuths(az, azi1);
                    double phi2 = getAngleFromAzimuths(azinv, azi1);

                    double coef = (fabs(sin(phi1)) + fabs(sin(phi2)))*dist;

                    addAngle.bindValue(":IDP",p);
                    addAngle.bindValue(":IDA1",ida1);
                    addAngle.bindValue(":IDA2",ida1);
                    addAngle.bindValue(":ANGLE",angle);
                    addAngle.bindValue(":COEF",coef);

                    if (! addAngle.exec()) {
                        pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et lui-même (arc boucle) : %2").arg(ida1).arg(addAngle.lastError().text()));
                        return false;
                    }
                }


                for (int a2 = a1 + 1; a2 < modelArcs->rowCount(); a2++) {

                    int ida2 = modelArcs->record(a2).value("IDA").toInt();
                    int pi2 = modelArcs->record(a2).value("PI").toInt();
                    int pf2 = modelArcs->record(a2).value("PF").toInt();
                    QVariant si2 = modelArcs->record(a2).value("SI");
                    QVariant sf2 = modelArcs->record(a2).value("SF");
                    double azi2 = modelArcs->record(a2).value("AI").toDouble();
                    double azf2 = modelArcs->record(a2).value("AF").toDouble();

                    if (pi1 == pi2 && pi1 == p) {
                        double angle = getAngleFromAzimuths(azi1, azi2);

                        calcAzimuth.bindValue(":S11", si1);
                        calcAzimuth.bindValue(":S21", si2);
                        calcAzimuth.bindValue(":S12", si1);
                        calcAzimuth.bindValue(":S22", si2);
                        calcAzimuth.bindValue(":S13", si1);
                        calcAzimuth.bindValue(":S23", si2);

                        if (! calcAzimuth.exec()) {
                            pLogger->ERREUR(QString("Récupération de l'azimuth et de la distance : %1").arg(calcAzimuth.lastError().text()));
                            return false;
                        }

                        QSqlQueryModel resAzDist;
                        resAzDist.setQuery(calcAzimuth);
                        double az = resAzDist.record(0).value("AZ").toDouble();
                        double azinv = resAzDist.record(0).value("AZinv").toDouble();
                        double dist = resAzDist.record(0).value("AZ").toDouble();

                        double phi1 = getAngleFromAzimuths(az, azi1);
                        double phi2 = getAngleFromAzimuths(azinv, azi2);

                        double coef = (fabs(sin(phi1)) + fabs(sin(phi2)))*dist;

                        addAngle.bindValue(":IDP",p);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);
                        addAngle.bindValue(":COEF",coef);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }

                    if (pf1 == pi2 && pf1 == p) {
                        double angle = getAngleFromAzimuths(azf1, azi2);

                        calcAzimuth.bindValue(":S11", sf1);
                        calcAzimuth.bindValue(":S21", si2);
                        calcAzimuth.bindValue(":S12", sf1);
                        calcAzimuth.bindValue(":S22", si2);
                        calcAzimuth.bindValue(":S13", sf1);
                        calcAzimuth.bindValue(":S23", si2);

                        if (! calcAzimuth.exec()) {
                            pLogger->ERREUR(QString("Récupération de l'azimuth et de la distance : %1").arg(calcAzimuth.lastError().text()));
                            return false;
                        }

                        QSqlQueryModel resAzDist;
                        resAzDist.setQuery(calcAzimuth);
                        double az = resAzDist.record(0).value("AZ").toDouble();
                        double azinv = resAzDist.record(0).value("AZinv").toDouble();
                        double dist = resAzDist.record(0).value("AZ").toDouble();

                        double phi1 = getAngleFromAzimuths(az, azf1);
                        double phi2 = getAngleFromAzimuths(azinv, azi2);

                        double coef = (fabs(sin(phi1)) + fabs(sin(phi2)))*dist;

                        addAngle.bindValue(":IDP",p);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);
                        addAngle.bindValue(":COEF",coef);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }

                    if (pi1 == pf2 && pi1 == p) {
                        double angle = getAngleFromAzimuths(azi1, azf2);

                        calcAzimuth.bindValue(":S11", si1);
                        calcAzimuth.bindValue(":S21", sf2);
                        calcAzimuth.bindValue(":S12", si1);
                        calcAzimuth.bindValue(":S22", sf2);
                        calcAzimuth.bindValue(":S13", si1);
                        calcAzimuth.bindValue(":S23", sf2);

                        if (! calcAzimuth.exec()) {
                            pLogger->ERREUR(QString("Récupération de l'azimuth et de la distance : %1").arg(calcAzimuth.lastError().text()));
                            return false;
                        }

                        QSqlQueryModel resAzDist;
                        resAzDist.setQuery(calcAzimuth);
                        double az = resAzDist.record(0).value("AZ").toDouble();
                        double azinv = resAzDist.record(0).value("AZinv").toDouble();
                        double dist = resAzDist.record(0).value("AZ").toDouble();

                        double phi1 = getAngleFromAzimuths(az, azi1);
                        double phi2 = getAngleFromAzimuths(azinv, azf2);

                        double coef = (fabs(sin(phi1)) + fabs(sin(phi2)))*dist;

                        addAngle.bindValue(":IDP",p);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);
                        addAngle.bindValue(":COEF",coef);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }

                    if (pf1 == pf2 && pf1 == p) {
                        double angle = getAngleFromAzimuths(azf1, azf2);

                        calcAzimuth.bindValue(":S11", sf1);
                        calcAzimuth.bindValue(":S21", sf2);
                        calcAzimuth.bindValue(":S12", sf1);
                        calcAzimuth.bindValue(":S22", sf2);
                        calcAzimuth.bindValue(":S13", sf1);
                        calcAzimuth.bindValue(":S23", sf2);

                        if (! calcAzimuth.exec()) {
                            pLogger->ERREUR(QString("Récupération de l'azimuth et de la distance : %1").arg(calcAzimuth.lastError().text()));
                            return false;
                        }

                        QSqlQueryModel resAzDist;
                        resAzDist.setQuery(calcAzimuth);
                        double az = resAzDist.record(0).value("AZ").toDouble();
                        double azinv = resAzDist.record(0).value("AZinv").toDouble();
                        double dist = resAzDist.record(0).value("AZ").toDouble();

                        double phi1 = getAngleFromAzimuths(az, azf1);
                        double phi2 = getAngleFromAzimuths(azinv, azf2);

                        double coef = (fabs(sin(phi1)) + fabs(sin(phi2)))*dist;

                        addAngle.bindValue(":IDP",p);
                        addAngle.bindValue(":IDA1",ida1);
                        addAngle.bindValue(":IDA2",ida2);
                        addAngle.bindValue(":ANGLE",angle);
                        addAngle.bindValue(":COEF",coef);

                        if (! addAngle.exec()) {
                            pLogger->ERREUR(QString("Ajout de l'angle entre l'arc %1 et l'arc %2 : %3").arg(ida1).arg(ida2).arg(addAngle.lastError().text()));
                            return false;
                        }
                    }
                }
            }

        }

        delete modelArcs;

        pLogger->INFO("--------------------------- build_PANGLES END -------------------------");
    } else {
        pLogger->INFO("------------------------ PANGLES already exists -----------------------");
    }

    return true;
}


//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DE VECTEURS M_PLACESARCS
//
//***************************************************************************************************************************************************

bool Graphe::build_PlacesArcs(){

    pLogger->INFO("------------------------ build_PlacesArcs START -------------------------");

    //REQUETE SUR PIF
    QSqlQueryModel* tablePIF = new QSqlQueryModel();
    tablePIF->setQuery("SELECT ida AS IDA, pi AS PI, pf AS PF FROM PIF;");

    if (tablePIF->lastError().isValid()) {
        pLogger->ERREUR(QString("tablePIF : %1").arg(tablePIF->lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel

    // Dimensionnement du vecteur
    m_PlacesArcs.resize(m_nbPlaces + 1);

    for(int j=0; j<m_nbArcs; j++){

        int ida = tablePIF->record(j).value("IDA").toInt();
        int pi = tablePIF->record(j).value("PI").toInt();
        int pf = tablePIF->record(j).value("PF").toInt();

        m_PlacesArcs[pi].push_back(ida);
        m_PlacesArcs[pf].push_back(ida);

    }//end for j

    delete tablePIF;

    pLogger->INFO("------------------------- build_PlacesArcs END --------------------------");

    return true;

}// END build_PlacesArcs

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DE VECTEURS M_ARC_ARCS
//
//***************************************************************************************************************************************************

bool Graphe::build_ArcArcs(){

    pLogger->INFO("------------------------ build_ArcArcs START -------------------------");

    m_ArcArcs.resize(m_nbArcs + 1);

    for(int idp=1; idp <= m_nbPlaces; idp++) {

        //ARCS TOUCHANT SOMMET INITIAL
        for(int i = 0; i < m_PlacesArcs[idp].size(); i++){

            int ida1 = m_PlacesArcs[idp][i];

            for(int j = i+1; j < m_PlacesArcs[idp].size(); j++){
                int ida2 = m_PlacesArcs[idp][j];

                // On Vérifie bien que les arcs n'ont pas déjà été identifiés comme voisin
                if (m_ArcArcs[ida1].indexOf(ida2) < 0) {
                    m_ArcArcs[ida1].push_back(ida2);
                    m_ArcArcs[ida2].push_back(ida1);
                }
            }
        }
    }

    pLogger->INFO("------------------------- build_ArcArcs END --------------------------");

    return true;

}// END build_ArcArcs

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE INFO EN BDD
//
//***************************************************************************************************************************************************

bool Graphe::insertINFO(){

    pLogger->INFO("-------------------------- insertINFO START --------------------------");

    if (! pDatabase->tableExists("INFO")) {
        QSqlQueryModel createTableINFO;
        createTableINFO.setQuery("CREATE TABLE INFO ( "
                                  "ID SERIAL NOT NULL PRIMARY KEY, "
                                  "methode integer, "
                                  "seuil_angle integer, "
                                  "LTOT float, "
                                  "NP integer, "
                                  "NA integer, "
                                  "NPA integer, "
                                  "N_INTER integer, "
                                  "N_IMP integer, "
                                  "N_COUPLES integer, "
                                  "N_CELIBATAIRES integer, "
                                  "NV integer, "

                                  "AVG_LVOIE float, "
                                  "STD_LVOIE float, "

                                  "AVG_ANG float, "
                                  "STD_ANG float, "

                                  "AVG_NBA float, "
                                  "STD_NBA float, "

                                  "AVG_NBP float, "
                                  "STD_NBP float, "

                                  "AVG_NBC float, "
                                  "STD_NBC float, "

                                  "AVG_O float, "
                                  "STD_O float, "

                                  "AVG_S float, "
                                  "STD_S float, "


                                  "AVG_LOG_LVOIE float, "
                                  "STD_LOG_LVOIE float, "

                                  "AVG_LOG_NBA float, "
                                  "STD_LOG_NBA float, "

                                  "AVG_LOG_NBP float, "
                                  "STD_LOG_NBP float, "

                                  "AVG_LOG_NBC float, "
                                  "STD_LOG_NBC float, "

                                  "AVG_LOG_O float, "
                                  "STD_LOG_O float, "

                                  "AVG_LOG_S float, "
                                  "STD_LOG_S float, "

                                  "ACTIVE boolean "
                                  ");");

        if (createTableINFO.lastError().isValid()) {
            pLogger->ERREUR("Impossible de créer la table INFO");
            pLogger->ERREUR(createTableINFO.lastError().text());
            return false;
        }

        QSqlQuery addInfos;
        addInfos.prepare("INSERT INTO INFO (NP, NA, NPA, N_INTER, N_IMP, ACTIVE) VALUES (:NP, :NA, :NPA, :N_INTER, :N_IMP, FALSE);");
        addInfos.bindValue(":NP",QVariant(m_nbPlaces));
        addInfos.bindValue(":NA",QVariant(m_nbArcs));
        addInfos.bindValue(":NPA",QVariant(m_nbPointsAnnexes));
        addInfos.bindValue(":N_INTER",QVariant(m_nbIntersections));
        addInfos.bindValue(":N_IMP",QVariant(m_nbImpasses));

        if (! addInfos.exec()) {
            pLogger->ERREUR("Impossible d'inserer les infos dans la table");
            pLogger->ERREUR(addInfos.lastError().text());
            return false;
        }
    }

    pLogger->INFO("-------------------------- insertINFO END ----------------------------");

    return true;

}//END create_info

//***************************************************************************************************************************************************

//***************************************************************************************************************************************************
//CONSTRUCTION DES TABLES DU GRAPHE
//
//***************************************************************************************************************************************************

bool Graphe::do_Graphe(QString brutArcsTableName){

    // Récupération des arcs bruts
    QSqlQueryModel *arcs_bruts = new QSqlQueryModel();

    arcs_bruts->setQuery(QString("SELECT "
                         "id AS IDA, "
                         "geom AS GEOM,"
                         "ST_NumPoints(geom) AS NUMPOINTS,"

                         "ST_StartPoint(geom) AS POINT_SI, "
                         "ST_EndPoint(geom) AS POINT_SF, "

                         "ST_azimuth(ST_StartPoint(geom), ST_PointN(geom,2)) AS AZIMUTH_I, "
                         "ST_azimuth(ST_EndPoint(geom), ST_PointN(geom,ST_NumPoints(geom)-1)) AS AZIMUTH_F "

                         "FROM %1").arg(brutArcsTableName));

    if (arcs_bruts->lastError().isValid()) {
        pLogger->ERREUR(QString("Récupération arcs_bruts : %1").arg(arcs_bruts->lastError().text()));
        return false;
    }//end if : test requête QSqlQueryModel


    //NOMBRE D'ARCS    (nombres de lignes du résultat de la requête)
    m_nbArcs = arcs_bruts->rowCount();

    pLogger->INFO(QString("Nombre d'arcs : %1").arg(m_nbArcs));

    if (! build_PIF(arcs_bruts)) {
        if (! pDatabase->dropTable("PIF")) {
            pLogger->ERREUR("build_PIF en erreur, ROLLBACK (drop PIF) échoué");
        } else {
            pLogger->INFO("build_PIF en erreur, ROLLBACK (drop PIF) réussi");
        }
        return false;
    }

    delete arcs_bruts;

    //construction des matrices attributs membres des arcs
    if (! build_PlacesArcs()) return false;
    if (! build_ArcArcs()) return false;

    if (! build_PANGLES()) {
        if (! pDatabase->dropTable("PANGLES")) {
            pLogger->ERREUR("build_PANGLES en erreur, ROLLBACK (drop PANGLES) échoué");
        } else {
            pLogger->INFO("build_PANGLES en erreur, ROLLBACK (drop PANGLES) réussi");
        }
        return false;
    }

    //table d'informations du graphe
    if (! insertINFO()) return false;

    return true;

}//end build_Graphe


bool Graphe::getPlacesOfArcs(int ida, int* pi, int* pf) {
    QSqlQuery queryArc;
    queryArc.prepare("SELECT PI, PF FROM PIF WHERE IDA = :IDA;");

    queryArc.bindValue(":IDA",ida);

    if (! queryArc.exec()) {
        pLogger->ERREUR(QString("Récupération des places de l'arc %1").arg(ida));
        return false;
    }

    queryArc.next();

    *pi = queryArc.record().value(0).toInt();
    *pf = queryArc.record().value(1).toInt();

    return true;
}

double Graphe::getAngle(int idp, int ida1, int ida2) {
    QSqlQuery queryAngle;
    queryAngle.prepare("SELECT ANGLE FROM PANGLES WHERE (IDP = :IDP) AND ((IDA1 = :IDARC11 AND IDA2 = :IDARC21)  OR (IDA2 = :IDARC12 AND IDA1 = :IDARC22));");

    queryAngle.bindValue(":IDARC11",ida1);
    queryAngle.bindValue(":IDARC21",ida2);
    queryAngle.bindValue(":IDARC12",ida1);
    queryAngle.bindValue(":IDARC22",ida2);
    queryAngle.bindValue(":IDS",idp);

    if (! queryAngle.exec()) {
        pLogger->ERREUR(QString("Récupération de l'angle entre les arcs %1 et %2 : %3").arg(ida1).arg(ida2).arg(queryAngle.lastError().text()));
        return -1;
    }

    double angle = -1;
    int nb_res = 0;

    if (queryAngle.next()) {

        angle = queryAngle.record().value(0).toDouble();
        nb_res++;

        while(queryAngle.next()){
            pLogger->INFO(QString("angle trouve entre les arcs %1 et %2 à la place %3 : %4").arg(ida1).arg(ida2).arg(idp).arg(angle));
            pLogger->INFO(QString("angle trouve entre les arcs %1 et %2 à la place %3 : %4").arg(ida1).arg(ida2).arg(idp).arg(queryAngle.record().value(0).toDouble()));

            nb_res ++;
            if(queryAngle.record().value(0).toDouble() < angle){
                angle = queryAngle.record().value(0).toDouble();
            }
        }

        if(nb_res != 1){
            pLogger->ATTENTION(QString("NOMBRE DE RESULTATS TROUVES entre les arcs %1 et %2 à la place %3 : %4 - ANGLE CHOISI : %5").arg(ida1).arg(ida2).arg(idp).arg(nb_res).arg(angle));
        }

    } else {
        pLogger->ERREUR(QString("Pas d'angle trouvé entre les arcs %1 et %2 à la place %3").arg(ida1).arg(ida2).arg(idp));
        return -1;
    }

    if (queryAngle.next()) {
        pLogger->ATTENTION(QString("Plusieurs angles trouvés entre les arcs %1 et %2 (arc boucle) à la place %3").arg(ida1).arg(ida2).arg(idp));
        //return -1;
    }

    return angle;
}


bool Graphe::checkAngle(int idp, int ida1, int ida2) {
    QSqlQuery queryAngle;
    queryAngle.prepare("UPDATE PANGLES SET USED=TRUE WHERE (IDP = :IDP) AND ((IDA1 = :IDARC11 AND IDA2 = :IDARC21) OR (IDA2 = :IDARC12 AND IDA1 = :IDARC22));");

    queryAngle.bindValue(":IDARC11",ida1);
    queryAngle.bindValue(":IDARC21",ida2);
    queryAngle.bindValue(":IDARC12",ida1);
    queryAngle.bindValue(":IDARC22",ida2);
    queryAngle.bindValue(":IDP",idp);

    if (! queryAngle.exec()) {
        pLogger->ERREUR(QString("Mise à jour de l'angle (USED) entre les arcs %1 et %2 à la place %3 : %4").arg(ida1).arg(ida2).arg(idp).arg(queryAngle.lastError().text()));
        return false;
    }

    return true;
}
