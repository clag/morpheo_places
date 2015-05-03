#include "Voies.h"

#include <iostream>
#include <fstream>
using namespace std;

#include <math.h>
#define PI 3.1415926535897932384626433832795

namespace WayMethods {

    int numMethods = 1;

    QString MethodeVoies_name[1] = {
        "Choix des couples par angles minimum"
    };
}

Voies::Voies(Database* db, Logger* log, Graphe* graphe, WayMethods::methodID methode, double seuil, QString rawTableName, QString directory)
{
    pDatabase = db;
    pLogger = log;

    pLogger->INFO(QString("Methode de contruction des voies : %1").arg(WayMethods::MethodeVoies_name[methode]));

    m_Graphe = graphe;
    m_rawTableName = rawTableName;
    m_directory = directory;
    m_seuil_angle = seuil;
    m_methode = methode;
    m_nbCouples = 0;
    m_nbCelibataire = 0;
    m_nbVoies = 0;
    m_length_tot = 0;

    // Les vecteurs dont on connait deja la taille sont dimensionnes
    m_Couples.resize(graphe->getNombreArcs() + 1);
    m_ArcVoies.resize(graphe->getNombreArcs() + 1);
    m_PlaceVoies.resize(graphe->getNombrePlaces() + 1);

    // Les vecteurs qui stockerons les voies n'ont pas une taille connue
    // On ajoute a la place d'indice 0 un vecteur vide pour toujours avoir :
    // indice dans le vecteur = identifiant de l'objet en base
    m_VoieArcs.push_back(QVector<long>(0));
    m_VoiePlaces.push_back(QVector<long>(0));
}

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DE VECTEURS M_COUPLES
//
//***************************************************************************************************************************************************

bool Voies::findCouplesAngleMin(int idp)
{
    QVector<long> stayingArcs;
    for (int a = 0; a < m_Graphe->getArcsOfPlace(idp)->size(); a++) {
        stayingArcs.push_back(m_Graphe->getArcsOfPlace(idp)->at(a));
    }

    QSqlQueryModel modelAngles;
    QSqlQuery queryAngles;
    queryAngles.prepare("SELECT IDA1, IDA2, ANGLE, COEF FROM PANGLES WHERE IDP = :IDP ORDER BY COEF;");
    queryAngles.bindValue(":IDP",idp);

    if (! queryAngles.exec()) {
        pLogger->ERREUR(QString("Recuperation arc dans findCouplesAngleMin : %1").arg(queryAngles.lastError().text()));
        return false;
    }

    modelAngles.setQuery(queryAngles);

    pLogger->DEBUG(QString("%1 angles").arg(modelAngles.rowCount()));

    for (int ang = 0; ang < modelAngles.rowCount(); ang++) {

        if (modelAngles.record(ang).value("ANGLE").toDouble() > m_seuil_angle * PI / 180) {
            continue;
        }

        int a1 = modelAngles.record(ang).value("IDA1").toInt();
        int a2 = modelAngles.record(ang).value("IDA2").toInt();

        int occ1 = stayingArcs.count(a1);
        int occ2 = stayingArcs.count(a2);

        if (occ1 == 0 || occ2 == 0) continue; // Un des arcs a deja ete traite
        if (a1 == a2 && occ1 < 2) continue; // arc boucle deja utilise, mais est en double dans le tableau des arcs

        m_Couples[a1].push_back(idp);
        m_Couples[a1].push_back(a2);
        m_Couples[a2].push_back(idp);
        m_Couples[a2].push_back(a1);

        m_nbCouples++;

        int idx = stayingArcs.indexOf(a1);
        if (idx == -1) {
            pLogger->ERREUR(QString("Pas normal de ne pas avoir l'arc %1 encore disponible à la place %2").arg(a1).arg(idp));
            return false;
        }
        stayingArcs.remove(idx);
        idx = stayingArcs.indexOf(a2);
        if (idx == -1) {
            pLogger->ERREUR(QString("Pas normal de ne pas avoir l'arc %1 encore disponible à la place %2").arg(a2).arg(idp));
            return false;
        }
        stayingArcs.remove(idx);
    }

    if (! stayingArcs.isEmpty()) {
        pLogger->DEBUG("Des celibataires !!");
        for (int sa = 0; sa < stayingArcs.size(); sa++) {
            long ida = stayingArcs.at(sa);
            m_Couples[ida].push_back(idp);
            m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
            m_Impasses.push_back(ida); // Ajout comme impasse
            m_nbCelibataire++;
        }
    }

    return true;
}





bool Voies::buildCouples(){

    pLogger->INFO("------------------------- buildCouples START -------------------------");

    QString degreefilename=QString("%1/degree_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
    QFile degreeqfile( degreefilename );
    if (! degreeqfile.open(QIODevice::ReadWrite) ) {
        pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les degrés");
        return false;
    }
    QTextStream degreestream( &degreeqfile );

    for(int idp = 1; idp <= m_Graphe->getNombrePlaces(); idp++){
        pLogger->DEBUG(QString("La place IDP %1").arg(idp));

        QVector<long>* arcsOfPlace = m_Graphe->getArcsOfPlace(idp);

        //nombre d'arcs candidats
        int N_arcs = arcsOfPlace->size();

        pLogger->DEBUG(QString("La place IDP %1 se trouve sur %2 arc(s)").arg(idp).arg(N_arcs));
        for(int a = 0; a < N_arcs; a++){
            pLogger->DEBUG(QString("     ID arcs : %1").arg(arcsOfPlace->at(a)));
        }//endfora

        //ECRITURE DU DEGRES DES SOMMETS
        //writing
        degreestream << idp;
        degreestream << " ";
        degreestream << N_arcs;
        degreestream << endl;

        //INITIALISATION

        if (N_arcs > 0) {

            //CALCUL DES COUPLES

            //si 1 seul arc passe par le sommet
            if(N_arcs == 1){
                long ida = arcsOfPlace->at(0);
                m_Couples[ida].push_back(idp);
                m_Couples[ida].push_back(0); //on l'ajoute comme arc terminal (en couple avec 0)
                m_Impasses.push_back(ida); // Ajout comme impasse
                m_nbCelibataire++;

                continue;
            }//endif 1

            //si 2 arcs passent par le sommet : ils sont ensemble !
            if(N_arcs == 2){
                long a1 = arcsOfPlace->at(0);
                long a2 = arcsOfPlace->at(1);

                m_Couples[a1].push_back(idp);
                m_Couples[a1].push_back(a2);
                m_Couples[a2].push_back(idp);
                m_Couples[a2].push_back(a1);
                m_nbCouples++;

                continue;
            }//endif 1

            if (! findCouplesAngleMin(idp)) return false;

        } else{
            pLogger->ERREUR(QString("La place %1 n'a pas d'arcs, donc pas de couple").arg(idp));
            return false;
        }//end else

    }//end for s

    //close the file
    degreeqfile.close();

    //NOMBRE DE COUPLES
    pLogger->INFO(QString("Nb TOTAL de Couples : %1").arg(m_nbCouples));
    pLogger->INFO(QString("Nb TOTAL de Célibataires : %1").arg(m_nbCelibataire));

    pLogger->INFO("-------------------------- buildCouples END --------------------------");

    // Tests de m_Couples
    for (int a = 1; a < m_Couples.size(); a++) {
        if (m_Couples.at(a).size() != 4) {
            pLogger->ERREUR(QString("L'arc %1 n'a pas une entree valide dans le tableau des couples (que %2 elements au lieu de 4)").arg(a).arg(m_Couples.at(a).size()));
            return false;
        }
        pLogger->DEBUG(QString("Arc %1").arg(a));
        pLogger->DEBUG(QString("    Arcs couples : %1 et %2").arg(m_Couples.at(a).at(1)).arg(m_Couples.at(a).at(3)));
    }

    return true;

}//END build_couples

//***************************************************************************************************************************************************
//CONSTRUCTION DU TABLEAU DES VECTEURS : M_VOIE_ARCS
//
//***************************************************************************************************************************************************

bool Voies::buildVectors(){

    pLogger->INFO("------------------------- buildVectors START -------------------------");

    // On parcourt la liste des impasses pour constituer les voies
    for (int i = 0; i < m_Impasses.size(); i++) {

        int ida_precedent = 0;
        int ida_courant = m_Impasses.at(i);
        if (m_Couples.at(ida_courant).size() == 5) {
            // Cette impasse a deja ete traitee : la voie a deja ete ajoutee
            continue;
        }

        int idv = m_nbVoies + 1;

        pLogger->DEBUG(QString("Construction de la voie %1...").arg(idv));
        pLogger->DEBUG(QString("... a partir de l'arc impasse %1").arg(ida_courant));

        QVector<long> voieA;
        QVector<long> voieS;

        int ids_courant = 0;
        if (m_Couples.at(ida_courant).at(1) == 0) {
            ids_courant = m_Couples.at(ida_courant).at(0);
        } else if (m_Couples.at(ida_courant).at(3) == 0) {
            ids_courant = m_Couples.at(ida_courant).at(2);
        } else {
            pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 doit etre une impasse").arg(ida_courant));
            return false;
        }

        if (! voieS.contains(ids_courant)) {
            voieS.push_back(ids_courant);
        }
        m_PlaceVoies[ids_courant].push_back(idv);

        while (ida_courant) {

            if (m_Couples.at(ida_courant).size() > 4) {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 est deja utilise par la voie %2").arg(ida_courant).arg(m_Couples.at(ida_courant).at(4)));
                return false;
            }

            m_Couples[ida_courant].push_back(idv);
            voieA.push_back(ida_courant);

            if (m_Couples.at(ida_courant).at(1) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(3);
                ids_courant = m_Couples.at(ida_precedent).at(2);
            } else if (m_Couples.at(ida_courant).at(3) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(1);
                ids_courant = m_Couples.at(ida_precedent).at(0);
            } else {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 et l'arc %2 devrait etre couples au sommet %3").arg(ida_courant).arg(ida_precedent).arg(ids_courant));
                pLogger->ERREUR(QString("Impossible de construire la voie %1").arg(idv));
                return false;
            }

            if (! voieS.contains(ids_courant)) {
                voieS.push_back(ids_courant);
            }
            m_PlaceVoies[ids_courant].push_back(idv);

        }

        m_VoieArcs.push_back(voieA);
        m_VoiePlaces.push_back(voieS);
        m_nbVoies++;
    }

    // Nous avons maintenant traite toutes les impasses. Ils restent potentiellement des arcs a traiter (le cas des boucles)
    for (int ida = 1; ida < m_Couples.size(); ida++) {
        if (m_Couples.at(ida).size() == 5) continue;

        int ida_courant = ida;
        int idv = m_nbVoies + 1;

        pLogger->DEBUG(QString("Construction de la voie %1...").arg(idv));
        pLogger->DEBUG(QString("... a partir de l'arc %1").arg(ida_courant));

        QVector<long> voieA;
        QVector<long> voieS;

        int ida_precedent = m_Couples.at(ida_courant).at(1);
        int ids_courant = m_Couples.at(ida_courant).at(0);

        if (! voieS.contains(ids_courant)) {
            voieS.push_back(ids_courant);
        }
        m_PlaceVoies[ids_courant].push_back(idv);

        while (ida_courant) {

            if (m_Couples.at(ida_courant).size() > 4) {
                pLogger->DEBUG("On est retombe sur un arc deja utilise : la boucle est bouclee");
                break;
            }

            m_Couples[ida_courant].push_back(idv);
            voieA.push_back(ida_courant);

            if (m_Couples.at(ida_courant).at(1) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(3);
                ids_courant = m_Couples.at(ida_precedent).at(2);
            } else if (m_Couples.at(ida_courant).at(3) == ida_precedent) {
                ida_precedent = ida_courant;
                ida_courant = m_Couples.at(ida_precedent).at(1);
                ids_courant = m_Couples.at(ida_precedent).at(0);
            } else {
                pLogger->ERREUR(QString("Mauvaise construction du vecteur des couples : l'arc %1 et l'arc %2 devrait etre couples au sommet %3").arg(ida_courant).arg(ida_precedent).arg(ids_courant));
                pLogger->ERREUR(QString("Impossible de construire la voie %1").arg(idv));
                return false;
            }

            if (! voieS.contains(ids_courant)) {
                voieS.push_back(ids_courant);
            }

            m_PlaceVoies[ids_courant].push_back(idv);

        }

        m_VoieArcs.push_back(voieA);
        m_VoiePlaces.push_back(voieS);
        m_nbVoies++;
    }

    pLogger->INFO(QString("Nombre de voies : %1").arg(m_nbVoies));

    // CONSTRUCTION DE VOIE-VOIES
    m_VoieVoies.resize(m_nbVoies + 1);
    for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++) {

        //on parcours les sommets sur la voie
        for(int s = 0; s < m_VoiePlaces[idv1].size(); s++){

            long ids = m_VoiePlaces[idv1][s];

            //on cherche les voies passant par ces sommets
            for(int v = 0; v < m_PlaceVoies[ids].size(); v++){

                long idv2 = m_PlaceVoies[ids][v];
                if(idv2 != idv1){
                    m_VoieVoies[idv1].push_back(idv2);
                }

            }//end for v (voies passant par les sommets sur la voie)

        }//end for s (sommets sur la voie)

    }//end for idv1 (voie)

    pLogger->INFO("-------------------------- buildVectors END --------------------------");

    for (int a = 1; a < m_Couples.size(); a++) {
        if (m_Couples.at(a).size() != 5) {
            pLogger->ERREUR(QString("L'arc %1 n'a pas une entree valide dans le tableau des couples (que %2 elements au lieu de 5)").arg(a).arg(m_Couples.at(a).size()));
            return false;
        }
    }

    return true;

}//END build_VoieArcs


//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE PVOIES EN BDD
//
//***************************************************************************************************************************************************

bool Voies::build_PVOIES(){

    bool voiesToDo = true;

    if (! pDatabase->tableExists("PVOIES")) {
        voiesToDo = true;
        pLogger->INFO("La table des PVOIES n'est pas en base, on la construit !");
    } else {
        // Test si la table PVOIES a deja ete construite avec les memes parametres methode + seuil
        QSqlQueryModel estMethodeActive;
        estMethodeActive.setQuery(QString("SELECT * FROM INFO WHERE methode = %1 AND seuil_angle = %2 AND ACTIVE=TRUE").arg((int) m_methode).arg((int) m_seuil_angle));
        if (estMethodeActive.lastError().isValid()) {
            pLogger->ERREUR(QString("Ne peut pas tester l'existance dans table de ce methode + seuil : %1").arg(estMethodeActive.lastError().text()));
            return false;
        }

        if (estMethodeActive.rowCount() > 0) {
            pLogger->INFO("La table des PVOIES est deja faite avec cette methode et ce seuil des angles");
            voiesToDo = false;
        } else {
            pLogger->INFO("La table des PVOIES est deja faite mais pas avec cette methode ou ce seuil des angles : on la refait");
            // Les voies existent mais n'ont pas ete faites avec la meme methode
            voiesToDo = true;

            if (! pDatabase->dropTable("PVOIES")) {
                pLogger->ERREUR("Impossible de supprimer PVOIES, pour la recreer avec la bonne methode");
                return false;
            }

            if (! pDatabase->dropTable("DTOPO_PVOIES")) {
                pLogger->ERREUR("Impossible de supprimer DTOPO_PVOIES, pour la recreer avec la bonne methode");
                return false;
            }
        }
    }

    //SUPPRESSION DE LA TABLE PVOIES SI DEJA EXISTANTE
    if (voiesToDo) {

        pLogger->INFO("-------------------------- build_PVOIES START ------------------------");

        // MISE A JOUR DE USED DANS PANGLES

        QSqlQuery queryAngle;
        queryAngle.prepare("UPDATE PANGLES SET USED=FALSE;");

        if (! queryAngle.exec()) {
            pLogger->ERREUR(QString("Mise à jour de l'angle (USED=FALSE) : %1").arg(queryAngle.lastError().text()));
            return false;
        }

        for (int a = 1; a < m_Couples.size(); a++) {

            int som1_couple = m_Couples.at(a).at(0);
            int a1 = m_Couples.at(a).at(1);

            if (a1 != 0) {
                if (! m_Graphe->checkAngle(som1_couple, a, a1)) return false;
            }

            int som2_couple = m_Couples.at(a).at(2);
            int a2 = m_Couples.at(a).at(3);
            if (a2 != 0) {
                if (! m_Graphe->checkAngle(som2_couple, a, a2)) return false;
            }
        }

        //CREATION DE LA TABLE PVOIES D'ACCUEIL DEFINITIVE

        QSqlQueryModel createPVOIES;
        createPVOIES.setQuery("CREATE TABLE PVOIES ( IDV SERIAL NOT NULL PRIMARY KEY, MULTIGEOM geometry, LENGTH float, NBA integer, NBP integer, NBC integer, NBC_P integer);");

        if (createPVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("createtable_voies : %1").arg(createPVOIES.lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //RECONSTITUTION DE LA GEOMETRIE DES VOIES

        QSqlQueryModel *geometryArcs = new QSqlQueryModel();

        geometryArcs->setQuery("SELECT IDA AS IDA, GEOM AS GEOM FROM PIF;");

        if (geometryArcs->lastError().isValid()) {
            pLogger->ERREUR(QString("req_arcsvoies : %1").arg(geometryArcs->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        int NA = m_Graphe->getNombreArcs();

        if(geometryArcs->rowCount() != NA){
            pLogger->ERREUR("Le nombre de lignes ne correspond pas /!\\ NOMBRE D'ARCS");
            return false;
        }

        QVector<QString> geometryVoies(m_nbVoies + 1);

        //pour toutes les voies
        for (int i = 0; i < geometryArcs->rowCount(); i++) {
            int ida = geometryArcs->record(i).value("IDA").toInt();
            QString geom = geometryArcs->record(i).value("GEOM").toString();
            long idv = m_Couples.at(ida).at(4);
            if (geometryVoies[idv].isEmpty()) {
                geometryVoies[idv] = "'" + geom + "'";
            } else {
                geometryVoies[idv] += ", '" + geom + "'";
            }
        }

        delete geometryArcs;

        QString geomfilename=QString("%1/geom_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile geomqfile( geomfilename );
        if (! geomqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les géométries");
            return false;
        }
        QTextStream geomstream( &geomqfile );


        // On ajoute toutes les voies, avec la geometrie (multi), le nombre d'arcs, de sommets et la connectivite
        for (int idv = 1; idv <= m_nbVoies; idv++) {

            int nba = m_VoieArcs.at(idv).size();
            int nbp = m_VoiePlaces.at(idv).size();


            // CALCUL DU NOMBRE DE CONNEXION DE LA VOIE
            int nbc = 0;
            int nbc_p = 0;

            // Pour chaque sommet appartenant a la voie, on regarde combien d'arcs lui sont connectes
            // On enleve les 2 arcs correspondant a la voie sur laquelle on se trouve

            for(int p = 0;  p < m_VoiePlaces[idv].size(); p++){
                nbc += m_Graphe->getArcsOfPlace(m_VoiePlaces[idv][p])->size()-2;
            }//end for

            // Dans le cas d'une non boucle, on a enleve a tord 2 arcs pour les fins de voies
            if ( nba + 1 == nbp ) { nbc += 2; nbc_p = nbc - 2; }
            else { nbc_p = nbc;}


            geomstream << idv;
            geomstream << " ";
            geomstream << geometryVoies.at(idv);
            geomstream << endl;



            QString addPVoie = QString("INSERT INTO PVOIES(IDV, MULTIGEOM, NBA, NBP, NBC, NBC_P) VALUES (%1, ST_LineMerge(ST_Union(ARRAY[%2])) , %3, %4, %5, %6);")
                    .arg(idv).arg(geometryVoies.at(idv)).arg(nba).arg(nbp).arg(nbc).arg(nbc_p);

            QSqlQuery addInPVOIES;
            addInPVOIES.prepare(addPVoie);

            if (! addInPVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter la voie %1 dans la table PVOIES : %2").arg(idv).arg(addInPVOIES.lastError().text()));
                return false;
            }

        }

        geomqfile.close();

        //On ajoute la voie correspondante à l'arc dans PIF

        for (int ida=1; ida < m_Couples.size(); ida++){
            int idv = m_Couples.at(ida).at(4);

            QSqlQuery addIDVAttInPIF;
            addIDVAttInPIF.prepare("UPDATE PIF SET IDV = :IDV WHERE ida = :IDA ;");
            addIDVAttInPIF.bindValue(":IDV",idv);
            addIDVAttInPIF.bindValue(":IDA",ida);

            if (! addIDVAttInPIF.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'identifiant %1 pour l'arc %2").arg(idv).arg(ida));
                pLogger->ERREUR(addIDVAttInPIF.lastError().text());
                return false;
            }

        }

        // On calcule la longueur des voies
        QSqlQuery updateLengthInPVOIES;
        updateLengthInPVOIES.prepare("UPDATE PVOIES SET LENGTH = ST_Length(MULTIGEOM);");

        if (! updateLengthInPVOIES.exec()) {
            pLogger->ERREUR(QString("Impossible de calculer la longueur de la voie dans la table PVOIES : %1").arg(updateLengthInPVOIES.lastError().text()));
            return false;
        }

        // On calcule la connectivite sur la longueur
        if (! pDatabase->add_att_div("PVOIES","LOC","LENGTH","NBC")) return false;

        if (! pDatabase->add_att_cl("PVOIES", "CL_NBC", "NBC", 10, true)) return false;
        if (! pDatabase->add_att_cl("PVOIES", "CL_LOC", "LOC", 10, true)) return false;


        pLogger->INFO("--------------------------- build_PVOIES END --------------------------");
    } else {
        pLogger->INFO("------------------------- PVOIES already exists -----------------------");
    }

    return true;
}

bool Voies::arcInVoie(long ida, long idv){

    //on parcourt tous les arcs de la voie
    for (int a2 = 0; a2 < m_VoieArcs.at(idv).size(); a2 ++){
        long ida2 = m_VoieArcs.at(idv).at(a2);

        if( ida2 == ida ){ return true; } //on a trouvé sur la voie l'arc en entrée

    }//end for

    return false;    //on n'a pas trouvé l'arc en entrée sur la voie

}//END arcInVoie

bool Voies::updatePIF(){

    //On ajoute la voie correspondante à l'arc dans PIF

    for (int ida=1; ida < m_Couples.size(); ida++){
        int idv = m_Couples.at(ida).at(4);

        QSqlQuery addIDVAttInPIF;
        addIDVAttInPIF.prepare("UPDATE PIF SET IDV = :IDV WHERE ida = :IDA ;");
        addIDVAttInPIF.bindValue(":IDV",idv);
        addIDVAttInPIF.bindValue(":IDA",ida);

        if (! addIDVAttInPIF.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer l'identifiant %1 pour l'arc %2").arg(idv).arg(ida));
            pLogger->ERREUR(addIDVAttInPIF.lastError().text());
            return false;
        }

    }

    QSqlQueryModel *structFromPVOIES = new QSqlQueryModel();

    structFromPVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE FROM PVOIES;");

    if (structFromPVOIES->lastError().isValid()) {
        pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans PVOIES : %1").arg(structFromPVOIES->lastError().text()));
        return false;
    }//end if : test requete QSqlQueryModel

    for(int v = 0; v < m_nbVoies; v++){
        int idv = structFromPVOIES->record(v).value("IDV").toInt();
        float struct_voie = structFromPVOIES->record(v).value("STRUCT_VOIE").toFloat();


        QSqlQuery addStructAttInPIF;
        addStructAttInPIF.prepare("UPDATE PIF SET STRUCT = :ST WHERE idv = :IDV ;");
        addStructAttInPIF.bindValue(":ST",struct_voie);
        addStructAttInPIF.bindValue(":IDV",idv);

        if (! addStructAttInPIF.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer la structuralité %1 pour la voie %2").arg(struct_voie).arg(idv));
            pLogger->ERREUR(addStructAttInPIF.lastError().text());
            return false;
        }


    }//end for v

    //SUPPRESSION DE L'OBJET
    delete structFromPVOIES;

    return true;
}//END updatePIF


//***************************************************************************************************************************************************
//CALCUL DES ATTRIBUTS
//
//***************************************************************************************************************************************************

bool Voies::calcStructuralite(){

    if (! pDatabase->columnExists("PVOIES", "DEGREE") || ! pDatabase->columnExists("PVOIES", "RTOPO") || ! pDatabase->columnExists("PVOIES", "STRUCT")) {
        pLogger->INFO("---------------------- calcStructuralite START ----------------------");

        // AJOUT DE L'ATTRIBUT DE STRUCTURALITE
        QSqlQueryModel addStructInPVOIES;
        addStructInPVOIES.setQuery("ALTER TABLE PVOIES ADD DEGREE integer, ADD RTOPO float, ADD STRUCT float;");

        if (addStructInPVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de structuralite dans PVOIES : %1").arg(addStructInPVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *lengthFromPVOIES = new QSqlQueryModel();

        lengthFromPVOIES->setQuery("SELECT IDV, length AS LENGTH_VOIE FROM PVOIES;");

        if (lengthFromPVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les longueurs dans PVOIES : %1").arg(lengthFromPVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        if(lengthFromPVOIES->rowCount() != m_nbVoies){
            pLogger->ERREUR("Le nombre de lignes ne correspond pas /!\\ NOMBRE DE VOIES ");
            return false;
        }

        //open the file
        QString lengthfilename=QString("%1/length_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile lengthqfile( lengthfilename );
        if (! lengthqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les longueurs");
            return false;
        }
        QTextStream lengthstream( &lengthqfile );

        float length_voie[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies; v++){
            int idv = lengthFromPVOIES->record(v).value("IDV").toInt();
            length_voie[idv]=lengthFromPVOIES->record(v).value("LENGTH_VOIE").toFloat();

            //writing
            lengthstream << idv;
            lengthstream << " ";
            lengthstream << length_voie[idv];
            lengthstream << endl;

            m_length_tot += length_voie[idv];
        }//end for v

        lengthqfile.close();

        //SUPPRESSION DE L'OBJET
        delete lengthFromPVOIES;

        pLogger->INFO(QString("LONGUEUR TOTALE DU RESEAU : %1 metres").arg(m_length_tot));


        //TRAITEMENT DES m_nbVoies VOIES

        //open the file dtopofile
        QString dtopofilename=QString("%1/dtopo_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile dtopoqfile( dtopofilename );
        if (! dtopoqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les distance topographiques");
            return false;
        }
        QTextStream dtopostream( &dtopoqfile );

        dtopostream << m_nbVoies << endl;

        //open the file
        QString adjacencyfilename=QString("%1/adjacency_%2_%3.txt").arg(m_directory).arg(QSqlDatabase::database().databaseName()).arg(m_rawTableName);
        QFile adjacencyqfile( adjacencyfilename );
        if (! adjacencyqfile.open(QIODevice::ReadWrite) ) {
            pLogger->ERREUR("Impossible d'ouvrir le fichier où écrire les adjacences");
            return false;
        }
        QTextStream adjacencystream( &adjacencyqfile );

        int nb_voies_supprimees = 0;

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1 ++) {

            pLogger->DEBUG(QString("*** VOIE V : %1").arg(idv1));
            pLogger->DEBUG(QString("*** LENGTH : %1").arg(length_voie[idv1]));

            //dtopofile << idv1 << " ";
            //adjacencyfile << idv1 << " ";

            //INITIALISATION
            float structuralite_v = 0;
            int rayonTopologique_v = 0;
            int dtopo = 0;
            int nb_voiestraitees = 0;
            int nb_voiestraitees_test = 0;
            QVector<int> V_ordreNombre;
            QVector<int> V_ordreLength;
            int dtopo_voies[m_nbVoies + 1];

            for(int i = 0; i < m_nbVoies + 1; i++){
                dtopo_voies[i]=-1;
            }//end for i

            dtopo_voies[idv1] = dtopo;
            nb_voiestraitees = 1;

            V_ordreNombre.push_back(1);
            V_ordreLength.push_back(length_voie[idv1]);

           //-------------------

            //TRAITEMENT
            while(nb_voiestraitees != m_nbVoies) {

                //cout<<endl<<"nb_voiestraitees : "<<nb_voiestraitees<<" / "<<m_nbVoies<<" voies."<<endl;
                nb_voiestraitees_test = nb_voiestraitees;

                //TRAITEMENT DE LA LIGNE ORDRE+1 DANS LES VECTEURS
                V_ordreNombre.push_back(0);
                V_ordreLength.push_back(0);

                //------------------------------------------voie idv2
                for(int idv2 = 1; idv2 < m_nbVoies + 1; idv2++){
                    //on cherche toutes les voies de l'ordre auquel on se trouve
                    if(dtopo_voies[idv2] == dtopo){

                        for (int v2 = 0; v2 < m_VoieVoies.at(idv2).size(); v2 ++) {                            
                            long idv3 = m_VoieVoies.at(idv2).at(v2);

                            // = si la voie n'a pas deja ete traitee
                            if (dtopo_voies[idv3] == -1){

                                dtopo_voies[idv3] = dtopo +1;
                                nb_voiestraitees += 1;
                                V_ordreNombre[dtopo+1] += 1;
                                V_ordreLength[dtopo+1] += length_voie[idv3];

                            }//end if (voie non traitee)
                        }//end for v2

                    }//end if (on trouve les voies de l'ordre souhaite)

                }//end for idv2 (voie)

                //si aucune voie n'a ete trouvee comme connectee a celle qui nous interesse
                if(nb_voiestraitees == nb_voiestraitees_test){
                    //cout<<"Traitement de la partie non connexe : "<<m_nbVoies-nb_voiestraitees<<" voies traitees / "<<m_nbVoies<<" voies."<<endl;


                    int nbvoies_connexe = 0;
                    for(int k=0; k<m_nbVoies; k++){
                        if(dtopo_voies[k]!=-1){
                            nbvoies_connexe += 1;
                        }
                        else{
                            nb_voiestraitees += 1;
                        }//end if
                    }//end for k

                    //SUPPRESSION DES VOIES NON CONNEXES AU GRAPHE PRINCIPAL

                    if(nbvoies_connexe < m_nbVoies/2){

                        QSqlQuery deletePVOIES;
                        deletePVOIES.prepare("DELETE FROM PVOIES WHERE idv = :IDV ;");
                        deletePVOIES.bindValue(":IDV",idv1 );

                        nb_voies_supprimees +=1;

                        if (! deletePVOIES.exec()) {
                            pLogger->ERREUR(QString("Impossible de supprimer la voie %1").arg(idv1));
                            pLogger->ERREUR(deletePVOIES.lastError().text());
                            return false;
                        }
                    }//end if nbvoies_connexe < m_nbVoies/2

                    break;

                }// end if nb_voiestraitees == nb_voiestraitees_test

                if(nb_voiestraitees == nb_voiestraitees_test){
                    pLogger->ERREUR(QString("Seulement %1 voies traitees sur %2").arg(nb_voiestraitees).arg(m_nbVoies));
                    return false;
                }//end if

                dtopo += 1;

            }//end while (voies a traitees)

            // On rend persistent toutes les structuralités calculées

            //CALCUL DE LA STRUCTURALITE
            for(int l=0; l<V_ordreLength.size(); l++){

                structuralite_v += l * V_ordreLength.at(l);

            }//end for l (calcul de la simpliest centrality)


            //CALCUL DU RAYON TOPO
            for(int l=0; l<V_ordreNombre.size(); l++){

                rayonTopologique_v += l * V_ordreNombre.at(l);

            }//end for l (calcul de l'ordre de la voie)

            //cout<<endl<<"*******VOIE IDV : "<<idv1<<endl;
            //for(int i=0; i <m_VoieVoies.at(idv1).size(); i++){
            //    cout<<"voie connectee : "<<m_VoieVoies.at(idv1).at(i)<<endl;
            //}

            //INSERTION EN BASE
            QSqlQuery addStructAttInPVOIES;
            addStructAttInPVOIES.prepare("UPDATE PVOIES SET DEGREE = :D, RTOPO = :RT, STRUCT = :S WHERE idv = :IDV ;");
            addStructAttInPVOIES.bindValue(":IDV",idv1 );
            addStructAttInPVOIES.bindValue(":D",m_VoieVoies.at(idv1).size());
            addStructAttInPVOIES.bindValue(":RT",rayonTopologique_v);
            addStructAttInPVOIES.bindValue(":S",structuralite_v);

            if (! addStructAttInPVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer la structuralite %1 et le rayon topo %2 pour la voie %3").arg(structuralite_v).arg(rayonTopologique_v).arg(idv1));
                pLogger->ERREUR(addStructAttInPVOIES.lastError().text());
                return false;
            }

            // Here we have the vector

            // dtopofile << "[";
            // adjacencyfile << "[";

            for(int i = 1; i < m_nbVoies + 1; i++){
                if (i <= idv1) {
                    dtopostream << dtopo_voies[i] << " ";
                }

                if(dtopo_voies[i] == -1 || dtopo_voies[i] == 0 || dtopo_voies[i] == 1){
                    adjacencystream << dtopo_voies[i];
                }
                else if(dtopo_voies[i] > 1){
                    adjacencystream << 0;
                }
                else{pLogger->ERREUR(QString("Erreur dans la matrice de distances topologiques"));}

                /*if(i != m_nbVoies){
                    dtopofile << ", ";
                    adjacencyfile << ", ";
                }
                else{*/
                    adjacencystream << " ";
                //}

            }//end for i

           // dtopofile << "]";
           // adjacencyfile << "]";

            dtopostream << endl;
            adjacencystream << endl;

        }//end for idv1

        m_nbVoies_supp = nb_voies_supprimees;


        dtopoqfile.close();
        adjacencyqfile.close();

        if (! pDatabase->add_att_div("PVOIES","SOL","STRUCT","LENGTH")) return false;

        if (! pDatabase->add_att_cl("PVOIES", "CL_S", "STRUCT", 10, true)) return false;
        if (! pDatabase->add_att_cl("PVOIES", "CL_SOL", "SOL", 10, true)) return false;

        if (! pDatabase->add_att_cl("PVOIES", "CL_RTOPO", "RTOPO", 10, true)) return false;

        if (! pDatabase->add_att_dif("PVOIES", "DIFF_CL", "CL_S", "CL_RTOPO")) return false;

        pLogger->INFO("------------------------ calcStructuralite END ----------------------");
    } else {
        pLogger->INFO("---------------- Struct attributes already in PVOIES -----------------");
    }

    return true;

}//END calcStructuralite



bool Voies::calcOrthoVoies(){

    if (! pDatabase->columnExists("PVOIES", "ORTHO")) {
        pLogger->INFO("---------------------- calcOrthoVoies START ----------------------");
        cout<<"---------------------- calcOrthoVoies START ----------------------"<<endl;

        // AJOUT DE L'ATTRIBUT D'ORTHOGONALITE
        QSqlQueryModel addOrthoInVOIES;
        addOrthoInVOIES.setQuery("ALTER TABLE PVOIES ADD ORTHO float;");

        if (addOrthoInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter l'attribut ortho dans PVOIES : %1").arg(addOrthoInVOIES.lastError().text()));
            return false;
        }



        //pour chaque voie
        for(int idv = 1; idv <= m_nbVoies ; idv++){

            float ortho = 0;

            //pour chaque sommet de la voie
            for(int sommet=0; sommet < m_VoiePlaces.at(idv).size(); sommet++){
                //on récupère les arcs liés au sommet
                int ids = m_VoiePlaces.at(idv).at(sommet);
                QVector<long>* arcsOfSommet = m_Graphe->getArcsOfPlace(ids);

                QVector<long> arcsInVoie;
                QVector<long> arcsOutVoie;

                //on cherche le ou les arcs sur la voie
                for(int i=0; i < arcsOfSommet->size(); i++) {
                    int arc = arcsOfSommet->at(i);

                    if (m_VoieArcs.at(idv).contains(arc)) {
                        arcsInVoie.push_back(arc);
                    } else {
                        arcsOutVoie.push_back(arc);
                    }
                }

                if (arcsOutVoie.size() == 0) {
                    continue;
                }

                QString ida1InOr = QString("ida1 = %1").arg(arcsInVoie.at(0));
                QString ida2InOr = QString("ida2 = %1").arg(arcsInVoie.at(0));

                for(int i = 1; i < arcsInVoie.size(); i++) {
                    ida1InOr.append(QString(" OR ida1 = %1").arg(arcsInVoie.at(i)));
                    ida2InOr.append(QString(" OR ida2 = %1").arg(arcsInVoie.at(i)));
                }


                for(int i=0; i < arcsOutVoie.size(); i++) {
                    int arc = arcsOutVoie.at(i);
                    QString req = QString("SELECT max(angle) AS maxdev FROM pangles WHERE idp = %1 AND ( (ida1 = %2 AND (%3) ) OR (ida2 = %2 AND (%4) ) );").arg(ids).arg(arc).arg(ida2InOr).arg(ida1InOr);

                    QSqlQueryModel reqAngleMax;
                    reqAngleMax.setQuery(req);

                    if (reqAngleMax.lastError().isValid()) {
                        pLogger->ERREUR(QString("Impossible de récupérer angle max : %1").arg(reqAngleMax.lastError().text()));
                        return false;
                    }

                    float maxAngle = reqAngleMax.record(0).value("maxdev").toFloat();
                    ortho += sin(PI - maxAngle);
                }

            }//end for sommet



            //INSERTION EN BASE

            QString addOrtho = QString("UPDATE PVOIES SET ORTHO = %1/nbc WHERE idv = %2 ;").arg(ortho).arg(idv);

            QSqlQuery addOrthoInVOIES;
            addOrthoInVOIES.prepare(addOrtho);

            if (! addOrthoInVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'ajouter l'orthogonalite %1 dans la table PVOIES : %2, erreur : %3").arg(ortho).arg(idv).arg(addOrthoInVOIES.lastError().text()));
                return false;
            }

        }//end for idv



        if (! pDatabase->add_att_cl("PVOIES", "CL_ORTHO", "ORTHO", 10, true)) return false;

        if (! pDatabase->add_att_div("PVOIES","ROO","RTOPO","ORTHO")) return false;
        if (! pDatabase->add_att_cl("PVOIES", "CL_ROO", "ROO", 10, true)) return false;



    } else {
        pLogger->INFO("---------------- Ortho attributes already in VOIES -----------------");
    }

    return true;

}//END calcOrthoVoies

bool Voies::calcUse(){

    if (! pDatabase->columnExists("PVOIES", "USE") || ! pDatabase->columnExists("PVOIES", "USE_MLT") || ! pDatabase->columnExists("PVOIES", "USE_LGT")) {
        pLogger->INFO("---------------------- calcUse START ----------------------");

        // AJOUT DE L'ATTRIBUT DE USE
        QSqlQueryModel addUseInVOIES;
        addUseInVOIES.setQuery("ALTER TABLE PVOIES ADD USE integer, ADD USE_MLT integer, ADD USE_LGT integer;");

        if (addUseInVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de use dans PVOIES : %1").arg(addUseInVOIES.lastError().text()));
            return false;
        }


        //CREATION DU TABLEAU COMPTEUR UNIQUE
        float voie_use[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_use[v] = 0;
        }//end for v

        //CREATION DU TABLEAU COMPTEUR MULTIPLE
        float voie_useMLT[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_useMLT[v] = 0;
        }//end for v

        //CREATION DU TABLEAU COMPTEUR DISTANCE
        float voie_useLGT[m_nbVoies + 1];
        for(int v = 0; v < m_nbVoies + 1; v++){
            voie_useLGT[v] = 0;
        }//end for v


        //CALCUL DU NOMBRE DE VOIES DE LA PARTIE CONNEXE
        QSqlQueryModel *nombreVOIES = new QSqlQueryModel();

        nombreVOIES->setQuery("SELECT COUNT(IDV) AS NBV FROM PVOIES;");

        if (nombreVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de compter le nombre de VOIES : %1").arg(nombreVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        if (nombreVOIES->rowCount() != 1) {
            pLogger->ERREUR(QString("Trop de réponses pour le nombre de voies : %1").arg(nombreVOIES->rowCount()));
            return false;
        }//end if : test requete QSqlQueryModel

        int nbVOIESconnexes = nombreVOIES->record(0).value("NBV").toInt();

        //SUPPRESSION DE L'OBJET
        delete nombreVOIES;


        //CREATION DU TABLEAU DONNANT LA LONGUEUR DE CHAQUE VOIE
        QSqlQueryModel *lengthFromPVOIES = new QSqlQueryModel();

        lengthFromPVOIES->setQuery("SELECT IDV, length AS LENGTH_VOIE FROM PVOIES;");

        if (lengthFromPVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les longueurs dans PVOIES : %1").arg(lengthFromPVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float length_voie[m_nbVoies + 1];

        for(int v = 0; v < m_nbVoies + 1; v++){
            length_voie[v] = -1;
        }//end for v

        for(int v = 0; v < m_nbVoies; v++){
            int idv = lengthFromPVOIES->record(v).value("IDV").toInt();
            length_voie[idv] = lengthFromPVOIES->record(v).value("LENGTH_VOIE").toFloat();
            m_length_tot += length_voie[idv];
        }//end for v

        //SI length_voie[v] = -1 ici ça veut dire que la voie n'était pas connexe et a été retirée

        //SUPPRESSION DE L'OBJET
        delete lengthFromPVOIES;



        //------------------------------------------voie i
        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1 ++){

            if (length_voie[idv1] != -1){ //La voie idv1 existe dans la table VOIE, elle est connexe au reste

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur unique
                float voie_parente[m_nbVoies + 1];
                int idv_fille;
                int idv_parent;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parente[v] = -1;
                }//end for v
                voie_parente[idv1] = 0;

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur multiple
                float voie_parenteMLT[m_nbVoies + 1];
                int idv_filleMLT;
                int idv_parentMLT;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parenteMLT[v] = -1;
                }//end for v
                voie_parenteMLT[idv1] = 0;

                //CREATION / MAJ DU TABLEAU DE PARENT pour compteur avec prise en compte de la longueur
                float voie_parenteLGT[m_nbVoies + 1];
                int idv_filleLGT;
                int idv_parentLGT;
                for(int v = 0; v < m_nbVoies + 1; v++){
                    voie_parenteLGT[v] = -1;
                }//end for v
                voie_parenteLGT[idv1] = 0;


                int dtopo = 0;
                int nb_voiestraitees = 0;
                int nb_voiestraitees_test = 0;

                //CREATION / MAJ DU TABLEAU donnant les distances topologiques de toutes les voies par rapport à la voie i
                int dtopo_voies[m_nbVoies + 1];
                for(int i = 0; i < m_nbVoies + 1; i++){
                    dtopo_voies[i] = -1;
                }//end for i

                //CREATION / MAJ DU TABLEAU donnant les longueurs d'accès de toutes les voies par rapport à la voie i
                int lacces_voies[m_nbVoies + 1];
                for(int i = 0; i < m_nbVoies + 1; i++){
                    lacces_voies[i]=0;
                }//end for i

                //traitement de la voie principale en cours (voie i)
                dtopo_voies[idv1] = dtopo;
                nb_voiestraitees = 1;


               while(nb_voiestraitees != nbVOIESconnexes){

                    nb_voiestraitees_test = nb_voiestraitees;

                    //------------------------------------------voie j
                    for(int idv2 = 1; idv2 < m_nbVoies + 1; idv2++){

                        if (length_voie[idv2] != -1){ //La voie idv2 existe dans la table VOIE, elle est connexe au reste

                            //on cherche toutes les voies de l'ordre auquel on se trouve
                            if(dtopo_voies[idv2] == dtopo){

                                for (int v2 = 0; v2 < m_VoieVoies.at(idv2).size(); v2 ++) {
                                    long idv3 = m_VoieVoies.at(idv2).at(v2);

                                     if (length_voie[idv3] != -1){ //La voie idv3 existe dans la table VOIE, elle est connexe au reste

                                        //si on est dans le cas d'un premier chemin le plus cout ou d'un même chemin double
                                        if(dtopo_voies[idv3] == -1 || dtopo_voies[idv3] == dtopo_voies[idv2] + 1){

                                            // on stocke le parent (cas MULTIPLE)
                                            voie_parenteMLT[idv3] = idv2;

                                            // on fait les comptes (cas MULTIPLE)
                                            idv_filleMLT = idv3 ;
                                            idv_parentMLT =  voie_parenteMLT[idv_filleMLT]  ;
                                            if(idv_parentMLT != idv2){
                                                pLogger->ERREUR(QString("Probleme de voie parente !"));
                                                return false;
                                            }
                                            while(voie_parenteMLT[idv_filleMLT] != 0){
                                                voie_useMLT[idv_parentMLT] += 1;
                                                idv_filleMLT = idv_parentMLT;
                                                idv_parentMLT = voie_parenteMLT[idv_filleMLT];

                                                if(idv_parentMLT == -1){
                                                    pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parentMLT = %1)").arg(idv_parentMLT));
                                                    return false;
                                                }
                                            }

                                        }

                                        // = SI LA VOIE A DEJA ETE TRAITEE
                                        if (dtopo_voies[idv3] != -1){

                                            //on compare les longueurs d'accès
                                            if(lacces_voies[idv3] > length_voie[idv2] + lacces_voies[idv2]){
                                                //on a trouvé un chemin plus court en distance ! (même si équivalent plus grand en distance topologique)

                                                //pLogger->ATTENTION(QString("Chemin plus court en distance pour la voie %1 par rapport à la voie %2 : %3 (plus court que %4)").arg(idv3).arg(idv1).arg(length_voie[idv2] + lacces_voies[idv2]).arg(lacces_voies[idv3]));

                                                idv_filleLGT = idv3 ;
                                                idv_parentLGT = voie_parenteLGT[idv_filleLGT];


                                                //si ce n'est pas la première fois
                                                if(idv_parentLGT != -1){

                                                    //pLogger->ATTENTION(QString("Ce n'est pas la première fois qu'un chemin plus court en distance est trouvé !"));


                                                    //IL FAUT ENLEVER L'INFO DANS USE AJOUTEE A TORD
                                                    //REMISE A NIVEAU DU USE
                                                    while(idv_parentLGT != 0){
                                                        voie_useLGT[idv_parentLGT] -= 1;
                                                        idv_filleLGT = idv_parentLGT;
                                                        idv_parentLGT = voie_parente[idv_filleLGT];

                                                        if(idv_parentLGT == -1){
                                                            pLogger->ERREUR(QString("REMISE A NIVEAU DU USE : Probleme de voie parente non remplie ! (idv_parentLGT = %1)").arg(idv_parentLGT));
                                                            return false;
                                                        }
                                                    }

                                                    //IL FAUT SUPPRIMER L'ANCIEN CHEMIN AJOUTE A TORD
                                                    //REMONTEE JUSQUA L'ANCETRE COMMUN

                                                 }

                                                //IL FAUT STOCKER LE NOUVEAU

                                                // on stocke le parent (cas DISTANCE MIN)
                                                voie_parenteLGT[idv3] = idv2;

                                                // on fait les comptes (cas DISTANCE MIN)
                                                idv_filleLGT = idv3 ;
                                                idv_parentLGT =  voie_parenteLGT[idv_filleLGT]  ;

                                                while(idv_parentLGT != 0){
                                                    voie_useLGT[idv_parentLGT] += 1;
                                                    idv_filleLGT = idv_parentLGT;
                                                    idv_parentLGT = voie_parente[idv_filleLGT];

                                                    if(idv_parentLGT == -1){
                                                       pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parentLGT = %1)").arg(idv_parentLGT));
                                                       return false;
                                                    }
                                                }


                                                /*else{ pLogger->ERREUR(QString("idv_parentLGT = %1)").arg(idv_parentLGT));
                                                      return false;
                                                };*/

                                            }// end if nouveau chemin plus court en distance

                                        }//end if déjà traitée


                                        // = SI LA VOIE N'A PAS DEJA ETE TRAITEE
                                        if (dtopo_voies[idv3] == -1){

                                            dtopo_voies[idv3] = dtopo +1;
                                            lacces_voies[idv3] = length_voie[idv2] + lacces_voies[idv2];
                                            nb_voiestraitees += 1;


                                            // on stocke le parent UNIQUE
                                            voie_parente[idv3] = idv2;

                                            // on fait les comptes UNIQUE
                                            idv_fille = idv3 ;
                                            idv_parent =  voie_parente[idv_fille]  ;
                                            if(idv_parent != idv2){
                                                pLogger->ERREUR(QString("Probleme de voie parente !"));
                                                return false;
                                            }
                                            while(voie_parente[idv_fille] != 0){
                                                voie_use[idv_parent] += 1;
                                                idv_fille = idv_parent;
                                                idv_parent = voie_parente[idv_fille];

                                                if(idv_parent == -1){
                                                    pLogger->ERREUR(QString("Probleme de voie parente non remplie ! (idv_parent = %1)").arg(idv_parent));
                                                    return false;
                                                }
                                            }

                                            /*//USE LGT = USE TOPO
                                            for(int v = 0; v < m_nbVoies + 1; v++){
                                                voie_parenteLGT[v] = voie_parente[v];
                                                voie_useLGT[v] = voie_use[v];
                                            }//end for v*/

                                        }//end if (voie non traitee)

                                     }//end if

                                }//end for v2 (idv3)

                            }//end if (on trouve les voies de l'ordre souhaite)

                        }//end if

                    }//end for idv2 : voie j

                    if(nb_voiestraitees == nb_voiestraitees_test && nb_voiestraitees != nbVOIESconnexes){
                        pLogger->ERREUR(QString("Seulement %1 voies traitees sur %2 pour idv %3").arg(nb_voiestraitees).arg(nbVOIESconnexes).arg(idv1));
                        return false;
                    }//end if

                    dtopo += 1;

                }//end while (voies a traitees)

            }//end if

        }//end for idv1

        //CALCUL DE USE
        for(int idv = 1; idv < m_nbVoies + 1; idv++){

            int use_v = voie_use[idv];
            int useMLT_v = voie_useMLT[idv];

            int useLGT_v = voie_useLGT[idv];
            if(voie_useLGT[idv] == 0){useLGT_v = voie_use[idv];}

            QSqlQuery addUseAttInPVOIES;
            addUseAttInPVOIES.prepare("UPDATE PVOIES SET USE = :USE, USE_MLT = :USE_MLT, USE_LGT = :USE_LGT WHERE idv = :IDV ;");
            addUseAttInPVOIES.bindValue(":IDV", idv );
            addUseAttInPVOIES.bindValue(":USE",use_v);
            addUseAttInPVOIES.bindValue(":USE_MLT",useMLT_v);
            addUseAttInPVOIES.bindValue(":USE_LGT",useLGT_v);

            if (! addUseAttInPVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'a structuralite'attribut use %1 pour la voie %2").arg(use_v).arg(idv));
                pLogger->ERREUR(addUseAttInPVOIES.lastError().text());
                return false;
            }

        }//end for idv

        if (! pDatabase->add_att_cl("PVOIES", "CL_USE", "USE", 10, true)) return false;

        if (! pDatabase->add_att_cl("PVOIES", "CL_USEMLT", "USE_MLT", 10, true)) return false;

        if (! pDatabase->add_att_cl("PVOIES", "CL_USELGT", "USE_LGT", 10, true)) return false;

        pLogger->INFO("------------------------ calcUse END ----------------------");

    } else {
        pLogger->INFO("---------------- Use attributes already in VOIES -----------------");
    }

    return true;

}//END calcUse

bool Voies::calcInclusion(){

    if (! pDatabase->columnExists("PVOIES", "INCL") || ! pDatabase->columnExists("PVOIES", "INCL_MOY")) {
        pLogger->INFO("---------------------- calcInclusion START ----------------------");

        // AJOUT DE L'ATTRIBUT D'INCLUSION
        QSqlQueryModel addInclInPVOIES;
        addInclInPVOIES.setQuery("ALTER TABLE PVOIES ADD INCL float, ADD INCL_MOY float;");

        if (addInclInPVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs d'inclusion dans PVOIES : %1").arg(addInclInPVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *structFromPVOIES = new QSqlQueryModel();

        structFromPVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE FROM PVOIES;");

        if (structFromPVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans PVOIES pour calculer l'inclusion: %1").arg(structFromPVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT LA STRUCTURALITE DE CHAQUE VOIE
        float struct_voie[m_nbVoies + 1];

        for(int i = 0; i < m_nbVoies + 1; i++){
            struct_voie[i] = 0;
        }

        for(int v = 0; v < m_nbVoies; v++){
            int idv = structFromPVOIES->record(v).value("IDV").toInt();
            struct_voie[idv]=structFromPVOIES->record(v).value("STRUCT_VOIE").toFloat();
            m_struct_tot += struct_voie[idv];
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete structFromPVOIES;

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++){

            float inclusion = 0;
            float inclusion_moy = 0;

            //on cherche toutes les voies connectées
            for (int v = 0; v < m_VoieVoies.at(idv1).size(); v ++) {
                long idv2 = m_VoieVoies.at(idv1).at(v);

                inclusion += struct_voie[idv2];
            }//end for idv2


            if(m_VoieVoies.at(idv1).size() != 0) {inclusion_moy = inclusion / m_VoieVoies.at(idv1).size();}
            else {inclusion_moy = 0;}

            //INSERTION EN BASE
            QSqlQuery addInclAttInPVOIES;
            addInclAttInPVOIES.prepare("UPDATE PVOIES SET INCL = :INCL, INCL_MOY = :INCL_MOY WHERE idv = :IDV ;");
            addInclAttInPVOIES.bindValue(":IDV",idv1 );
            addInclAttInPVOIES.bindValue(":INCL",inclusion);
            addInclAttInPVOIES.bindValue(":INCL_MOY",inclusion_moy);

            if (! addInclAttInPVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'inclusion %1 et l'inclusion moyenne %2 pour la voie %3").arg(inclusion).arg(inclusion_moy).arg(idv1));
                pLogger->ERREUR(addInclAttInPVOIES.lastError().text());
                return false;
            }

        }//end for idv1

        if (! pDatabase->add_att_cl("PVOIES", "CL_INCL", "INCL", 10, true)) return false;
        if (! pDatabase->add_att_cl("PVOIES", "CL_INCLMOY", "INCL_MOY", 10, true)) return false;

        pLogger->INFO(QString("STRUCTURALITE TOTALE SUR LE RESEAU : %1").arg(m_struct_tot));

    } else {
        pLogger->INFO("---------------- Incl attributes already in PVOIES -----------------");
    }

    return true;

}//END calcInclusion

bool Voies::calcLocalAccess(){

    if (! pDatabase->columnExists("PVOIES", "LOCAL_ACCESS1") || ! pDatabase->columnExists("PVOIES", "LOCAL_ACCESS2") || ! pDatabase->columnExists("PVOIES", "LOCAL_ACCESS3")) {
        pLogger->INFO("---------------------- calcLocalAccess START ----------------------");

        // AJOUT DE L'ATTRIBUT D'ACCESSIBILITE LOCALE
        QSqlQueryModel addLAInPVOIES;
        addLAInPVOIES.setQuery("ALTER TABLE PVOIES ADD LOCAL_ACCESS1 integer, ADD LOCAL_ACCESS2 integer, ADD LOCAL_ACCESS3 integer;");

        if (addLAInPVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs d'accessibilité locale dans PVOIES : %1").arg(addLAInPVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel* degreeFromPVOIES = new QSqlQueryModel();

        degreeFromPVOIES->setQuery("SELECT IDV, DEGREE AS DEGREE_VOIE FROM PVOIES;");

        if (degreeFromPVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les degres dans PVOIES pour calculer l'acessibilite locale: %1").arg(degreeFromPVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT LA STRUCTURALITE DE CHAQUE VOIE
        float degree_voie[m_nbVoies + 1];

        for(int i = 0; i < m_nbVoies + 1; i++){
            degree_voie[i] = 0;
        }

        for(int v = 0; v < m_nbVoies; v++){
            int idv = degreeFromPVOIES->record(v).value("IDV").toInt();
            degree_voie[idv]=degreeFromPVOIES->record(v).value("DEGREE_VOIE").toFloat();
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete degreeFromPVOIES;

        for(int idv1 = 1; idv1 < m_nbVoies + 1; idv1++){

            float loc_acc_1 = 0;
            float loc_acc_2 = 0;
            float loc_acc_3 = 0;

            //on cherche toutes les voies connectées à idv1
            for (int v2 = 0; v2 < m_VoieVoies.at(idv1).size(); v2 ++) {
                long idv2 = m_VoieVoies.at(idv1).at(v2);
                loc_acc_1 += degree_voie[idv2];
                loc_acc_2 += degree_voie[idv2];
                loc_acc_3 += degree_voie[idv2];

                //on cherche toutes les voies connectées à idv2
                for (int v3 = 0; v3 < m_VoieVoies.at(idv2).size(); v3 ++) {
                    long idv3 = m_VoieVoies.at(idv2).at(v3);
                    loc_acc_2 += degree_voie[idv3];
                    loc_acc_3 += degree_voie[idv3];

                    //on cherche toutes les voies connectées à idv3
                    for (int v4 = 0; v4 < m_VoieVoies.at(idv3).size(); v4 ++) {
                        long idv4= m_VoieVoies.at(idv3).at(v4);
                        loc_acc_3 += degree_voie[idv4];

                    }//end for v4

                }//end for v3

            }//end for v2


            //INSERTION EN BASE
            QSqlQuery addInclAttInPVOIES;
            addInclAttInPVOIES.prepare("UPDATE PVOIES SET LOCAL_ACCESS1 = :LA1, LOCAL_ACCESS2 = :LA2, LOCAL_ACCESS3 = :LA3 WHERE idv = :IDV ;");
            addInclAttInPVOIES.bindValue(":IDV",idv1 );
            addInclAttInPVOIES.bindValue(":LA1",loc_acc_1);
            addInclAttInPVOIES.bindValue(":LA2",loc_acc_2);
            addInclAttInPVOIES.bindValue(":LA3",loc_acc_3);

            if (! addInclAttInPVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer l'accessibilite locale 1 %1 2 %2 et 3 %3 pour la voie %4").arg(loc_acc_1).arg(loc_acc_2).arg(loc_acc_3).arg(idv1));
                pLogger->ERREUR(addInclAttInPVOIES.lastError().text());
                return false;
            }

        }//end for idv1

        if (! pDatabase->add_att_cl("PVOIES", "CL_LA1", "LOCAL_ACCESS1", 10, true)) return false;
        if (! pDatabase->add_att_cl("PVOIES", "CL_LA2", "LOCAL_ACCESS2", 10, true)) return false;
        if (! pDatabase->add_att_cl("PVOIES", "CL_LA3", "LOCAL_ACCESS3", 10, true)) return false;

    } else {
        pLogger->INFO("---------------- Local Access attributes already in PVOIES -----------------");
    }

    return true;

}//END calcLocalAccess

bool Voies::calcBruitArcs(){

    if (! pDatabase->columnExists("PIF", "BRUIT_1") || ! pDatabase->columnExists("PIF", "BRUIT_2")  || ! pDatabase->columnExists("PIF", "BRUIT_3")  || ! pDatabase->columnExists("PIF", "BRUIT_4")) {
        pLogger->INFO("---------------------- calcBruitArcs START ----------------------");

        // AJOUT DE L'ATTRIBUT DE BRUIT
        QSqlQueryModel addBruitInPIF;
        addBruitInPIF.setQuery("ALTER TABLE PIF ADD BRUIT_1 float, ADD BRUIT_2 float, ADD BRUIT_3 float, ADD BRUIT_4 float;");

        if (addBruitInPIF.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de bruit dans SIF : %1").arg(addBruitInPIF.lastError().text()));
            return false;
        }

        QSqlQueryModel *attFromSIF = new QSqlQueryModel();

        attFromSIF->setQuery("SELECT IDA, SI, SF, IDV, ST_length(GEOM) as LENGTH FROM PIF;");

        if (attFromSIF->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les attributs dans SIF: %1").arg(attFromSIF->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        int nbArcs = attFromSIF->rowCount();

        //CREATION DU TABLEAU DONNANT LES ATTRIBUTS DE CHAQUE ARC
        int idv_arc[nbArcs + 1];
        int si_arc[nbArcs + 1];
        int sf_arc[nbArcs + 1];
        float length_arc[nbArcs + 1];

        for(int i = 0; i < nbArcs + 1; i++){
            idv_arc[i] = 0;
            si_arc[i] = 0;
            sf_arc[i] = 0;
            length_arc[i] = 0;
        }

        for(int a = 0; a < nbArcs; a++){
            int ida = attFromSIF->record(a).value("IDA").toInt();
            idv_arc[ida]=attFromSIF->record(a).value("IDV").toInt();
            si_arc[ida]=attFromSIF->record(a).value("SI").toInt();
            sf_arc[ida]=attFromSIF->record(a).value("SF").toInt();
            length_arc[ida]=attFromSIF->record(a).value("LENGTH").toInt();
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete attFromSIF;

        for(int ida1 = 1; ida1 < nbArcs + 1; ida1++){

            float bruit_1 = 0;
            float bruit_2 = 0;
            float bruit_3 = 0;
            float bruit_4 = 0;

            int arc_comb1 = 0;
            int arc_comb2 = 0;
            int arc_comb3 = 0;
            int arc_comb4 = 0;

            //on parcourt l'ensemble des arcs du tableau
            for (int ida2 = 1; ida2 < nbArcs + 1; ida2 ++) {

                if((ida1 != ida2 && ida2 != arc_comb1 && ida2 != arc_comb2 && ida2 != arc_comb3) //l'arc est différent de ceux qu'on a déjà
                && (idv_arc[ida1] == idv_arc[ida2]) // les deux arcs sont sur la même voie
                && (si_arc[ida1]==si_arc[ida2] || si_arc[ida1]==sf_arc[ida2] || sf_arc[ida1]==sf_arc[ida2] || sf_arc[ida1]==si_arc[ida2]) // les deux arcs partagent un sommet
                && length_arc[ida1] != 0
                && length_arc[ida2] != 0
                ){

                    pLogger->INFO(QString("*************Arc de base : %1").arg(ida1));

                        if(bruit_1 == 0){
                            bruit_1 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb1 = ida2;
                            pLogger->INFO(QString("Combine 1 : %1, bruit : %2").arg(arc_comb1).arg(bruit_1));
                        }
                        else if(bruit_2 == 0){
                            bruit_2 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb2 = ida2;
                            pLogger->INFO(QString("Combine 2 : %1, bruit : %2").arg(arc_comb2).arg(bruit_2));
                        }
                        else if(bruit_3 == 0){ // cas d'une boucle
                            bruit_3 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb3 = ida2;
                            pLogger->INFO(QString("Combine 3 : %1, bruit : %2").arg(arc_comb3).arg(bruit_3));

                        }
                        else if(bruit_4 == 0){ // cas d'une boucle
                            bruit_4 = length_arc[ida1] / (length_arc[ida1] + length_arc[ida2]);
                            arc_comb4 = ida2;
                            pLogger->INFO(QString("Combine 4 : %1, bruit : %2").arg(arc_comb4).arg(bruit_4));

                        }else{
                        pLogger->ERREUR(QString("ATTENTION CAS PARTICULIER : les quatre bruits sont remplis : B1 = %1 (arc %2) ; B2 = %3 (arc %4) ; B3 = %5 (arc %6) pour l'arc ida1 = %4").arg(bruit_1).arg(arc_comb1).arg(bruit_2).arg(arc_comb2).arg(bruit_3).arg(arc_comb3).arg(ida1));
                        //return false;
                        }
                }

            }//end for a2


            //INSERTION EN BASE
            QSqlQuery addBruitAttInPIF;
            addBruitAttInPIF.prepare("UPDATE PIF SET BRUIT_1 = :B1, BRUIT_2 = :B2, BRUIT_3 = :B3, BRUIT_4 = :B4 WHERE ida = :IDA ;");
            addBruitAttInPIF.bindValue(":IDA",ida1 );
            addBruitAttInPIF.bindValue(":B1",bruit_1);
            addBruitAttInPIF.bindValue(":B2",bruit_2);
            addBruitAttInPIF.bindValue(":B3",bruit_3);
            addBruitAttInPIF.bindValue(":B4",bruit_4);

            if (! addBruitAttInPIF.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer les bruits 1 %1, 2 %2 et 3 %3 pour l'arc' %4").arg(bruit_1).arg(bruit_2).arg(bruit_3).arg(ida1));
                pLogger->ERREUR(addBruitAttInPIF.lastError().text());
                return false;
            }

        }//end for ida1

    } else {
        pLogger->INFO("---------------- Bruit already in PIF -----------------");
    }

    return true;

}//END calcBruitArcs


bool Voies::calcGradient(){

    if (! pDatabase->columnExists("PVOIES", "GRAD") || ! pDatabase->columnExists("PVOIES", "GRAD_MOY")) {
        pLogger->INFO("---------------------- calcGradient START ----------------------");

        // AJOUT DE L'ATTRIBUT DE GRADIENT
        QSqlQueryModel addGradlInPVOIES;
        addGradlInPVOIES.setQuery("ALTER TABLE PVOIES ADD GRAD float, ADD GRAD_MOY float;");

        if (addGradlInPVOIES.lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible d'ajouter les attributs de gradient dans PVOIES : %1").arg(addGradlInPVOIES.lastError().text()));
            return false;
        }

        QSqlQueryModel *structFromPVOIES = new QSqlQueryModel();

        structFromPVOIES->setQuery("SELECT IDV, STRUCT AS STRUCT_VOIE FROM PVOIES;");

        if (structFromPVOIES->lastError().isValid()) {
            pLogger->ERREUR(QString("Impossible de recuperer les structuralites dans PVOIES pour calculer le gradient : %1").arg(structFromPVOIES->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        //CREATION DU TABLEAU DONNANT LA STRUCTURALITE DE CHAQUE VOIE
        float struct_voie[m_nbVoies + 1];

        for(int i = 0; i < m_nbVoies + 1; i++){
            struct_voie[i] = 0;
        }

        for(int v = 0; v < m_nbVoies; v++){
            int idv = structFromPVOIES->record(v).value("IDV").toInt();
            struct_voie[idv]=structFromPVOIES->record(v).value("STRUCT_VOIE").toFloat();
            m_struct_tot += struct_voie[idv];
        }//end for v

        //SUPPRESSION DE L'OBJET
        delete structFromPVOIES;

        //***********************************************Calcul du gradient :

        for(int idv0 = 1; idv0 < m_nbVoies + 1; idv0++){

            float gradient = 0;
            int size_idv3 = 0;
            float gradient_moy = 0;

            //on cherche toutes les voies connectées à idv0
            for (int v1 = 0; v1 < m_VoieVoies.at(idv0).size(); v1 ++) {
                long idv1 = m_VoieVoies.at(idv0).at(v1);

                gradient += fabs(struct_voie[idv0]-struct_voie[idv1]);
                size_idv3 += 1;

                //on cherche toutes les voies connectées à idv1
                for (int v2 = 0; v2 < m_VoieVoies.at(idv1).size(); v2 ++) {
                    long idv2 = m_VoieVoies.at(idv1).at(v2);

                    gradient += fabs(struct_voie[idv0]-struct_voie[idv2]);
                    size_idv3 += 1;

                    //on cherche toutes les voies connectées à idv2
                    for (int v3 = 0; v3 < m_VoieVoies.at(idv2).size(); v3 ++) {
                        long idv3 = m_VoieVoies.at(idv2).at(v3);


                        gradient += fabs(struct_voie[idv0]-struct_voie[idv3]);
                        size_idv3 += 1;


                    }//end for idv3


                }//end for idv2


            }//end for idv1


            if(size_idv3 != 0) {gradient_moy = gradient / size_idv3;}
            else {gradient_moy = 0;}

            //INSERTION EN BASE
            QSqlQuery addInclAttInPVOIES;
            addInclAttInPVOIES.prepare("UPDATE PVOIES SET GRAD = :GRAD, GRAD_MOY = :GRAD_MOY WHERE idv = :IDV ;");
            addInclAttInPVOIES.bindValue(":IDV",idv0 );
            addInclAttInPVOIES.bindValue(":GRAD",gradient);
            addInclAttInPVOIES.bindValue(":GRAD_MOY",gradient_moy);

            if (! addInclAttInPVOIES.exec()) {
                pLogger->ERREUR(QString("Impossible d'inserer le gradient %1 et le gradient moyenne %2 pour la voie %3").arg(gradient).arg(gradient_moy).arg(idv0));
                pLogger->ERREUR(addInclAttInPVOIES.lastError().text());
                return false;
            }

        }//end for idv0

        if (! pDatabase->add_att_cl("PVOIES", "CL_GRAD", "GRAD", 10, true)) return false;
        if (! pDatabase->add_att_cl("PVOIES", "CL_GRADMOY", "GRAD_MOY", 10, true)) return false;

        pLogger->INFO(QString("STRUCTURALITE TOTALE SUR LE RESEAU : %1").arg(m_struct_tot));

    } else {
        pLogger->INFO("---------------- Grad attributes already in PVOIES -----------------");
    }

    return true;

}//END calcGradient







//***************************************************************************************************************************************************
//INSERT NV ATT DE LA TABLE INFO EN BDD
//
//***************************************************************************************************************************************************

bool Voies::insertINFO(){

    pLogger->INFO("-------------------------- start insertINFO --------------------------");

    //STOCKAGE DES INFOS*******

    if (! pDatabase->tableExists("INFO")){
        pLogger->ERREUR("La table INFO doit etre presente au moment des voies");
        return false;
    }//endif

    // Test si existe methode + seuil

    QSqlQueryModel alreadyInTable;
    alreadyInTable.setQuery(QString("SELECT * FROM INFO WHERE methode = %1 AND seuil_angle = %2").arg((int) m_methode).arg((int) m_seuil_angle));
    if (alreadyInTable.lastError().isValid()) {
        pLogger->ERREUR(QString("Ne peut pas tester l'existance dans table de cette methode et seuil : %1").arg(alreadyInTable.lastError().text()));
        return false;
    }

    bool isInTable = (alreadyInTable.rowCount() > 0);
    bool isActive = false;
    if (isInTable) isActive = alreadyInTable.record(0).value("active").toBool();

    if (isInTable && isActive) {
        pLogger->INFO(QString("Methode = %1 et seuil = %2 est deja dans la table INFO, et est actif").arg(m_methode).arg(m_seuil_angle));
        return true;
    } else if (isInTable && ! isActive) {
        QSqlQuery desactiveTous;
        desactiveTous.prepare("UPDATE INFO SET ACTIVE=FALSE;");

        if (! desactiveTous.exec()) {
            pLogger->ERREUR(QString("Impossible de desactiver toutes les configuratiosn de la table info : %1").arg(desactiveTous.lastError().text()));
            return false;
        }

        QSqlQuery activeUn;
        activeUn.prepare(QString("UPDATE INFO SET ACTIVE=TRUE WHERE methode = %1 AND seuil_angle = %2").arg((int) m_methode).arg((int) m_seuil_angle));

        if (! activeUn.exec()) {
            pLogger->ERREUR(QString("Impossible d'activer la bonne configuration toutes les configuration de la table info : %1").arg(activeUn.lastError().text()));
            return false;
        }

    } else {
        // La configuration n'est pas dans la table INFO
        QSqlQuery desactiveTous;
        desactiveTous.prepare("UPDATE INFO SET ACTIVE=FALSE;");

        if (! desactiveTous.exec()) {
            pLogger->ERREUR(QString("Impossible de desactiver toutes les configuration de la table info : %1").arg(desactiveTous.lastError().text()));
            return false;
        }

        QSqlQueryModel *req_voies_avg = new QSqlQueryModel();
        req_voies_avg->setQuery(  "SELECT "

                                  "AVG(nba) as AVG_NBA, "
                                  "AVG(nbp) as AVG_NBP, "
                                  "AVG(nbc) as AVG_NBC, "
                                  "AVG(rtopo) as AVG_O, "
                                  "AVG(struct) as AVG_S, "

                                  "STDDEV(length) as STD_L, "
                                  "STDDEV(nba) as STD_NBA, "
                                  "STDDEV(nbp) as STD_NBP, "
                                  "STDDEV(nbc) as STD_NBC, "
                                  "STDDEV(rtopo) as STD_O, "
                                  "STDDEV(struct) as STD_S, "

                                  "AVG(LOG(length)) as AVG_LOG_L, "
                                  "AVG(LOG(nba)) as AVG_LOG_NBA, "
                                  "AVG(LOG(nbp)) as AVG_LOG_NBP, "
                                  "AVG(LOG(nbc)) as AVG_LOG_NBC, "
                                  "AVG(LOG(rtopo)) as AVG_LOG_O, "
                                  "AVG(LOG(struct)) as AVG_LOG_S, "

                                  "STDDEV(LOG(length)) as STD_LOG_L, "
                                  "STDDEV(LOG(nba)) as STD_LOG_NBA, "
                                  "STDDEV(LOG(nbp)) as STD_LOG_NBP, "
                                  "STDDEV(LOG(nbc)) as STD_LOG_NBC, "
                                  "STDDEV(LOG(rtopo)) as STD_LOG_O, "
                                  "STDDEV(LOG(struct)) as STD_LOG_S "

                                  "FROM PVOIES "

                                  "WHERE length > 0 AND nba > 0 AND nbp > 0 AND nbc > 0 AND rtopo > 0 AND struct > 0;");

        if (req_voies_avg->lastError().isValid()) {
            pLogger->ERREUR(QString("create_info - req_voies_avg : %1").arg(req_voies_avg->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float avg_nba = req_voies_avg->record(0).value("AVG_NBA").toFloat();
        float avg_nbp = req_voies_avg->record(0).value("AVG_NBP").toFloat();
        float avg_nbc = req_voies_avg->record(0).value("AVG_NBC").toFloat();
        float avg_o = req_voies_avg->record(0).value("AVG_O").toFloat();
        float avg_s = req_voies_avg->record(0).value("AVG_S").toFloat();

        float std_length = req_voies_avg->record(0).value("STD_L").toFloat();
        float std_nba = req_voies_avg->record(0).value("STD_NBA").toFloat();
        float std_nbp = req_voies_avg->record(0).value("STD_NBP").toFloat();
        float std_nbc = req_voies_avg->record(0).value("STD_NBC").toFloat();
        float std_o = req_voies_avg->record(0).value("STD_O").toFloat();
        float std_s = req_voies_avg->record(0).value("STD_S").toFloat();

        float avg_log_length = req_voies_avg->record(0).value("AVG_LOG_L").toFloat();
        float avg_log_nba = req_voies_avg->record(0).value("AVG_LOG_NBA").toFloat();
        float avg_log_nbp = req_voies_avg->record(0).value("AVG_LOG_NBP").toFloat();
        float avg_log_nbc = req_voies_avg->record(0).value("AVG_LOG_NBC").toFloat();
        float avg_log_o = req_voies_avg->record(0).value("AVG_LOG_O").toFloat();
        float avg_log_s = req_voies_avg->record(0).value("AVG_LOG_S").toFloat();

        float std_log_length = req_voies_avg->record(0).value("STD_LOG_L").toFloat();
        float std_log_nba = req_voies_avg->record(0).value("STD_LOG_NBA").toFloat();
        float std_log_nbp = req_voies_avg->record(0).value("STD_LOG_NBP").toFloat();
        float std_log_nbc = req_voies_avg->record(0).value("STD_LOG_NBC").toFloat();
        float std_log_o = req_voies_avg->record(0).value("STD_LOG_O").toFloat();
        float std_log_s = req_voies_avg->record(0).value("STD_LOG_S").toFloat();

        //SUPPRESSION DE L'OBJET
        delete req_voies_avg;

        QSqlQueryModel *req_angles_avg = new QSqlQueryModel();
        req_angles_avg->setQuery( "SELECT "
                                  "AVG(angle) as AVG_ANG, "
                                  "STDDEV(angle) as STD_ANG "

                                  "FROM PANGLES WHERE USED;");

        if (req_angles_avg->lastError().isValid()) {
            pLogger->ERREUR(QString("create_info - req_angles_avg : %1").arg(req_angles_avg->lastError().text()));
            return false;
        }//end if : test requete QSqlQueryModel

        float avg_ang = req_angles_avg->record(0).value("AVG_ANG").toFloat();
        float std_ang = req_angles_avg->record(0).value("STD_ANG").toFloat();

        //SUPPRESSION DE L'OBJET
        delete req_angles_avg;

        pLogger->INFO(QString("m_length_tot : %1").arg(m_length_tot));

        QSqlQuery info_in_db;
        info_in_db.prepare("INSERT INTO INFO ("

                           "methode, seuil_angle, LTOT, N_COUPLES, N_CELIBATAIRES, NV, "
                           "AVG_LVOIE, STD_LVOIE, AVG_ANG, STD_ANG, AVG_NBA, STD_NBA, AVG_NBP, STD_NBP, AVG_NBC, STD_NBC, AVG_O, STD_O, AVG_S, STD_S, "
                           "AVG_LOG_LVOIE, STD_LOG_LVOIE, AVG_LOG_NBA, STD_LOG_NBA, AVG_LOG_NBP, STD_LOG_NBP, AVG_LOG_NBC, STD_LOG_NBC, AVG_LOG_O, STD_LOG_O, AVG_LOG_S, STD_LOG_S, "
                           "ACTIVE) "

                           "VALUES ("

                           ":M, :SA, :LTOT, :N_COUPLES, :N_CELIBATAIRES, :NV, "
                           ":AVG_LVOIE, :STD_LVOIE, :AVG_ANG, :STD_ANG, :AVG_NBA, :STD_NBA, :AVG_NBP, :STD_NBP, :AVG_NBC, :STD_NBC, :AVG_O, :STD_O, :AVG_S, :STD_S, "
                           ":AVG_LOG_LVOIE, :STD_LOG_LVOIE, :AVG_LOG_NBA, :STD_LOG_NBA, :AVG_LOG_NBP, :STD_LOG_NBP, :AVG_LOG_NBC, :STD_LOG_NBC, :AVG_LOG_O, :STD_LOG_O, :AVG_LOG_S, :STD_LOG_S, "
                           "TRUE);");

        info_in_db.bindValue(":M",m_methode);
        info_in_db.bindValue(":SA",m_seuil_angle);
        info_in_db.bindValue(":LTOT",QVariant(m_length_tot));
        info_in_db.bindValue(":N_COUPLES",QVariant(m_nbCouples));
        info_in_db.bindValue(":N_CELIBATAIRES",QVariant(m_nbCelibataire));
        info_in_db.bindValue(":NV",QVariant(m_nbVoies));

        info_in_db.bindValue(":AVG_LVOIE",QVariant(m_length_tot/m_nbVoies));

        info_in_db.bindValue(":AVG_ANG",avg_ang);
        info_in_db.bindValue(":STD_ANG",std_ang);

        info_in_db.bindValue(":AVG_NBA",avg_nba);
        info_in_db.bindValue(":AVG_NBP",avg_nbp);
        info_in_db.bindValue(":AVG_NBC",avg_nbc);
        info_in_db.bindValue(":AVG_O",avg_o);
        info_in_db.bindValue(":AVG_S",avg_s);

        info_in_db.bindValue(":STD_LVOIE",std_length);
        info_in_db.bindValue(":STD_NBA",std_nba);
        info_in_db.bindValue(":STD_NBA",std_nba);
        info_in_db.bindValue(":STD_NBP",std_nbp);
        info_in_db.bindValue(":STD_NBC",std_nbc);
        info_in_db.bindValue(":STD_O",std_o);
        info_in_db.bindValue(":STD_S",std_s);

        info_in_db.bindValue(":AVG_LOG_LVOIE",avg_log_length);
        info_in_db.bindValue(":AVG_LOG_NBA",avg_log_nba);
        info_in_db.bindValue(":AVG_LOG_NBP",avg_log_nbp);
        info_in_db.bindValue(":AVG_LOG_NBC",avg_log_nbc);
        info_in_db.bindValue(":AVG_LOG_O",avg_log_o);
        info_in_db.bindValue(":AVG_LOG_S",avg_log_s);

        info_in_db.bindValue(":STD_LOG_LVOIE",std_log_length);
        info_in_db.bindValue(":STD_LOG_NBA",std_log_nba);
        info_in_db.bindValue(":STD_LOG_NBA",std_log_nba);
        info_in_db.bindValue(":STD_LOG_NBP",std_log_nbp);
        info_in_db.bindValue(":STD_LOG_NBC",std_log_nbc);
        info_in_db.bindValue(":STD_LOG_O",std_log_o);
        info_in_db.bindValue(":STD_LOG_S",std_log_s);


        if (! info_in_db.exec()) {
            pLogger->ERREUR(QString("Impossible d'inserer les infos (sur PVOIES) dans la table INFO : %1").arg(info_in_db.lastError().text()));
            return false;
        }

    }

    pLogger->INFO("--------------------------- end insertINFO ---------------------------");

    return true;

}//END insertINFO

//***************************************************************************************************************************************************

//***************************************************************************************************************************************************
//CONSTRUCTION DE LA TABLE DES PVOIES
//
//***************************************************************************************************************************************************

bool Voies::do_Voies(){

    // Construction des couples d'arcs
    if (! buildCouples()) return false;

    // Construction des attributs membres des voies
    if (! buildVectors()) return false;

    // Construction de la table PVOIES en BDD
    if (! build_PVOIES()) {
        if (! pDatabase->dropTable("PVOIES")) {
            pLogger->ERREUR("build_PVOIES en erreur, ROLLBACK (drop PVOIES) echoue");
        } else {
            pLogger->INFO("build_PVOIES en erreur, ROLLBACK (drop PVOIES) reussi");
        }
        return false;
    }

    return true;

}//end do_Voie

//***************************************************************************************************************************************************
//INSERTION DES ATTRIBUTS DES PVOIES
//
//***************************************************************************************************************************************************

bool Voies::do_Att_Arc(){

        if (! calcBruitArcs()) {
            if (! pDatabase->dropColumn("PIF", "BRUIT_1")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_1) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_1) reussi");
            }
            if (! pDatabase->dropColumn("PIF", "BRUIT_2")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_2) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_2) reussi");
            }
            if (! pDatabase->dropColumn("PIF", "BRUIT_3")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) reussi");
            }
            if (! pDatabase->dropColumn("PIF", "BRUIT_4")) {
                pLogger->ERREUR("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) echoue");
            } else {
                pLogger->INFO("calcBruitArcs en erreur, ROLLBACK (drop column BRUIT_3) reussi");
            }
            return false;
        }

        return true;

}//end do_Att_Arc

bool Voies::do_Att_Voie(bool connexion, bool use, bool inclusion, bool gradient, bool local_access){

    // Calcul de la structuralite
    if (! calcStructuralite()) {
        if (! pDatabase->dropColumn("PVOIES", "DEGREE")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column DEGREE) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column DEGREE) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "RTOPO")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column RTOPO) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column RTOPO) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "STRUCT")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column STRUCT) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column STRUCT) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "CL_S")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column CL_S) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column CL_S) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "SOL")) {
            pLogger->ERREUR("calcStructuralite en erreur, ROLLBACK (drop column SOL) echoue");
        } else {
            pLogger->INFO("calcStructuralite en erreur, ROLLBACK (drop column SOL) reussi");
        }
        return false;
    }


    if( ! calcOrthoVoies()){
        if (! pDatabase->dropColumn("VOIES", "ORTHO")) {
            pLogger->ERREUR("calcOrthoVoies en erreur, ROLLBACK (drop column ORTHO) echoue");
        } else {
            pLogger->INFO("calcOrthoVoies en erreur, ROLLBACK (drop column ORTHO) reussi");
        }
        return false;
    }

    //calcul des utilisation (betweenness)
    if (use && ! calcUse()) {
        if (! pDatabase->dropColumn("PVOIES", "USE")) {
            pLogger->ERREUR("calcUse en erreur, ROLLBACK (drop column USE) echoue");
        } else {
            pLogger->INFO("calcUse en erreur, ROLLBACK (drop column USE) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "USE_MLT")) {
            pLogger->ERREUR("calcUse en erreur, ROLLBACK (drop column USE_MLT) echoue");
        } else {
            pLogger->INFO("calcUse en erreur, ROLLBACK (drop column USE_MLT) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "USE_LGT")) {
            pLogger->ERREUR("calcUse en erreur, ROLLBACK (drop column USE_LGT) echoue");
        } else {
            pLogger->INFO("calcUse en erreur, ROLLBACK (drop column USE_LGT) reussi");
        }
        return false;
    }

    // Calcul de l'inclusion
    if (inclusion && ! calcInclusion()) {
        if (! pDatabase->dropColumn("PVOIES", "INCL")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column INCL) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column INCL) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "INCL_MOY")) {
            pLogger->ERREUR("calcInclusion en erreur, ROLLBACK (drop column INCL_MOY) echoue");
        } else {
            pLogger->INFO("calcInclusion en erreur, ROLLBACK (drop column INCL_MOY) reussi");
        }
        return false;
    }

    // Calcul de l'accebilité locale
    if(local_access && ! calcLocalAccess() ){
        if (! pDatabase->dropColumn("PVOIES", "LOCAL_ACCESS1")) {
            pLogger->ERREUR("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS1) echoue");
        } else {
            pLogger->INFO("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS1) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "LOCAL_ACCESS2")) {
            pLogger->ERREUR("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS2) echoue");
        } else {
            pLogger->INFO("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS2) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "LOCAL_ACCESS3")) {
            pLogger->ERREUR("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS3) echoue");
        } else {
            pLogger->INFO("calcLocalAccess en erreur, ROLLBACK (drop column LOCAL_ACCESS3) reussi");
        }
        return false;
    }

    // Calcul du gradient
    if (gradient && ! calcGradient()) {
        if (! pDatabase->dropColumn("PVOIES", "GRAD")) {
            pLogger->ERREUR("calcGradient en erreur, ROLLBACK (drop column GRAD) echoue");
        } else {
            pLogger->INFO("calcGradient en erreur, ROLLBACK (drop column GRAD) reussi");
        }
        if (! pDatabase->dropColumn("PVOIES", "GRAD_MOY")) {
            pLogger->ERREUR("calcGradient en erreur, ROLLBACK (drop column GRAD_MOY) echoue");
        } else {
            pLogger->INFO("calcGradient en erreur, ROLLBACK (drop column GRAD_MOY) reussi");
        }
        return false;
    }


    //construction de la table PVOIES en BDD
    if (! insertINFO()) return false;

    //upadte PIF
    if (! updatePIF()) return false;

    return true;

}//end do_Att_Voie
