#include "modelsud.h"
#include "brauhelfer.h"
#include "modelausruestung.h"
#include "modelnachgaerverlauf.h"
#include <math.h>
#include <qmath.h>

ModelSud::ModelSud(Brauhelfer *bh, QSqlDatabase db) :
    SqlTableModel(bh, db),
    bh(bh),
    mUpdating(false),
    mSkipUpdateOnOtherModelChanged(false),
    swWzMaischenRecipe(QVector<double>()),
    swWzKochenRecipe(QVector<double>()),
    swWzGaerungRecipe(QVector<double>()),
    swWzGaerungCurrent(QVector<double>())
{
    connect(this, SIGNAL(modelReset()), this, SLOT(onModelReset()));
    connect(this, SIGNAL(rowsInserted(const QModelIndex&,int,int)), this, SLOT(onModelReset()));
    connect(this, SIGNAL(rowsRemoved(const QModelIndex&,int,int)), this, SLOT(onModelReset()));
    connect(this, SIGNAL(rowChanged(const QModelIndex&)), this, SLOT(onRowChanged(const QModelIndex&)));
    mVirtualField.append("SWIst");
    mVirtualField.append("SREIst");
    mVirtualField.append("MengeIst");
    mVirtualField.append("IbuIst");
    mVirtualField.append("FarbeIst");
    mVirtualField.append("CO2Ist");
    mVirtualField.append("Spundungsdruck");
    mVirtualField.append("Gruenschlauchzeitpunkt");
    mVirtualField.append("SpeiseNoetig");
    mVirtualField.append("SpeiseAnteil");
    mVirtualField.append("ZuckerAnteil");
    mVirtualField.append("Woche");
    mVirtualField.append("ReifezeitDelta");
    mVirtualField.append("AbfuellenBereitZutaten");
    mVirtualField.append("MengeSollKochbeginn");
    mVirtualField.append("MengeSollKochende");
    mVirtualField.append("WuerzemengeAnstellenTotal");
    mVirtualField.append("SW_Malz");
    mVirtualField.append("SW_WZ_Maischen");
    mVirtualField.append("SW_WZ_Kochen");
    mVirtualField.append("SW_WZ_Gaerung");
    mVirtualField.append("SWSollKochbeginn");
    mVirtualField.append("SWSollKochbeginnMitWz");
    mVirtualField.append("SWSollKochende");
    mVirtualField.append("SWSollAnstellen");
    mVirtualField.append("VerdampfungszifferIst");
    mVirtualField.append("sEVG");
    mVirtualField.append("tEVG");
    mVirtualField.append("Alkohol");
    mVirtualField.append("AnlageVerdampfungsziffer");
    mVirtualField.append("AnlageSudhausausbeute");
    mVirtualField.append("RestalkalitaetFaktor");
    mVirtualField.append("FaktorHauptgussEmpfehlung");
    mVirtualField.append("BewertungMittel");
}

void ModelSud::createConnections()
{
    connect(bh->modelMalzschuettung(), SIGNAL(rowChanged(const QModelIndex&)),
            this, SLOT(onOtherModelRowChanged(const QModelIndex&)));
    connect(bh->modelHopfengaben(), SIGNAL(rowChanged(const QModelIndex&)),
            this, SLOT(onOtherModelRowChanged(const QModelIndex&)));
    connect(bh->modelHefegaben(), SIGNAL(rowChanged(const QModelIndex&)),
            this, SLOT(onOtherModelRowChanged(const QModelIndex&)));
    connect(bh->modelWeitereZutatenGaben(), SIGNAL(rowChanged(const QModelIndex&)),
            this, SLOT(onOtherModelRowChanged(const QModelIndex&)));
    connect(bh->modelAusruestung(), SIGNAL(rowChanged(const QModelIndex&)),
            this, SLOT(onAnlageRowChanged(const QModelIndex&)));
    connect(bh->modelWasser(), SIGNAL(rowChanged(const QModelIndex&)),
            this, SLOT(onWasserRowChanged(const QModelIndex&)));
}

void ModelSud::onModelReset()
{
    int rows = rowCount();
    swWzMaischenRecipe = QVector<double>(rows);
    swWzKochenRecipe = QVector<double>(rows);
    swWzGaerungRecipe = QVector<double>(rows);
    swWzGaerungCurrent = QVector<double>(rows);
    for (int row = 0; row < rows; ++row)
        updateSwWeitereZutaten(row);
    emit modified();
}

void ModelSud::onRowChanged(const QModelIndex &idx)
{
    update(idx.row());
}

void ModelSud::onOtherModelRowChanged(const QModelIndex &idx)
{
    if (mSkipUpdateOnOtherModelChanged)
        return;
    const SqlTableModel* model = static_cast<const SqlTableModel*>(idx.model());
    int row = getRowWithValue(ColID, model->data(idx.row(), model->fieldIndex("SudID")));
    update(row);
}

void ModelSud::onAnlageRowChanged(const QModelIndex &idx)
{
    QVariant name = bh->modelAusruestung()->data(idx.row(), ModelAusruestung::ColName);
    for (int row = 0; row < rowCount(); ++row)
    {
        if (data(row, ColAnlage) == name)
            update(row);
    }
}

void ModelSud::onWasserRowChanged(const QModelIndex &idx)
{
    QVariant name = bh->modelWasser()->data(idx.row(), ModelWasser::ColName);
    for (int row = 0; row < rowCount(); ++row)
    {
        if (data(row, ColWasserprofil) == name)
            update(row);
    }
}

QVariant ModelSud::dataExt(const QModelIndex &idx) const
{
    switch(idx.column())
    {
    case ColBraudatum:
    {
        return QDateTime::fromString(QSqlTableModel::data(idx).toString(), Qt::ISODate);
    }
    case ColAbfuelldatum:
    {
        return QDateTime::fromString(QSqlTableModel::data(idx).toString(), Qt::ISODate);
    }
    case ColErstellt:
    {
        return QDateTime::fromString(QSqlTableModel::data(idx).toString(), Qt::ISODate);
    }
    case ColGespeichert:
    {
        return QDateTime::fromString(QSqlTableModel::data(idx).toString(), Qt::ISODate);
    }
    case ColSWIst:
    {
        return data(idx.row(), ColSWAnstellen).toDouble() + swWzGaerungCurrent[idx.row()];
    }
    case ColSREIst:
    {
        if (data(idx.row(), ColSchnellgaerprobeAktiv).toBool())
            return data(idx.row(), ColSWSchnellgaerprobe).toDouble();
        else
            return data(idx.row(), ColSWJungbier).toDouble();
    }
    case ColMengeIst:
    {
        Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
        switch (status)
        {
        case Brauhelfer::SudStatus::Rezept:
            return data(idx.row(), ColMenge).toDouble();
        case Brauhelfer::SudStatus::Gebraut:
            return data(idx.row(), ColWuerzemengeAnstellen).toDouble();
        case Brauhelfer::SudStatus::Abgefuellt:
        case Brauhelfer::SudStatus::Verbraucht:
            return data(idx.row(), Colerg_AbgefuellteBiermenge).toDouble();
        }
    }
    case ColIbuIst:
    {
        double mengeIst = data(idx.row(), ColMengeIst).toDouble();
        if (mengeIst <= 0.0)
            return 0.0;
        double mengeFaktor = data(idx.row(), ColMenge).toDouble() / mengeIst;
        return data(idx.row(), ColIBU).toDouble() * mengeFaktor;
    }
    case ColFarbeIst:
    {
        double mengeIst = data(idx.row(), ColMengeIst).toDouble();
        if (mengeIst <= 0.0)
            return 0.0;
        double mengeFaktor = data(idx.row(), ColMenge).toDouble() / mengeIst;
        return data(idx.row(), Colerg_Farbe).toDouble() * mengeFaktor;
    }
    case ColCO2Ist:
    {
        return bh->modelNachgaerverlauf()->getLastCO2(data(idx.row(), ColID));
    }
    case ColSpundungsdruck:
    {
        double co2 = data(idx.row(), ColCO2).toDouble();
        double T = data(idx.row(), ColTemperaturJungbier).toDouble();
        return BierCalc::spundungsdruck(co2, T);
    }
    case ColGruenschlauchzeitpunkt:
    {
        double co2Soll = data(idx.row(), ColCO2).toDouble();
        double sw = data(idx.row(), ColSWIst).toDouble();
        double T = data(idx.row(), ColTemperaturJungbier).toDouble();
        double sre = data(idx.row(), ColSWSchnellgaerprobe).toDouble();
        return BierCalc::gruenschlauchzeitpunkt(co2Soll, sw, sre, T);
    }
    case ColSpeiseNoetig:
    {
        double co2Soll = data(idx.row(), ColCO2).toDouble();
        double sw = data(idx.row(), ColSWIst).toDouble();
        double sreJungbier = data(idx.row(), ColSWJungbier).toDouble();
        double T = data(idx.row(), ColTemperaturJungbier).toDouble();
        double sreSchnellgaerprobe = data(idx.row(), ColSREIst).toDouble();
        double jungbiermenge = data(idx.row(), ColJungbiermengeAbfuellen).toDouble();
        return BierCalc::speise(co2Soll, sw, sreSchnellgaerprobe, sreJungbier, T) * jungbiermenge * 1000;
    }
    case ColSpeiseAnteil:
    {
        if (data(idx.row(), ColSpunden).toBool())
            return 0.0;
        double speiseVerfuegbar = data(idx.row(), ColSpeisemenge).toDouble() * 1000;
        double speiseNoetig = data(idx.row(), ColSpeiseNoetig).toDouble();
        if (speiseNoetig == std::numeric_limits<double>::infinity())
            return speiseVerfuegbar;
        if (speiseNoetig > speiseVerfuegbar)
            speiseNoetig = speiseVerfuegbar;
        return speiseNoetig;
    }
    case ColZuckerAnteil:
    {
        if (data(idx.row(), ColSpunden).toBool())
            return 0.0;
        double co2Soll = data(idx.row(), ColCO2).toDouble();
        double sw = data(idx.row(), ColSWIst).toDouble();
        double sreJungbier = data(idx.row(), ColSWJungbier).toDouble();
        double T = data(idx.row(), ColTemperaturJungbier).toDouble();
        double sreSchnellgaerprobe = data(idx.row(), ColSREIst).toDouble();
        double jungbiermenge = data(idx.row(), ColJungbiermengeAbfuellen).toDouble();
        double zucker = BierCalc::zucker(co2Soll, sw, sreSchnellgaerprobe, sreJungbier, T) * jungbiermenge;
        double speiseNoetig = data(idx.row(), ColSpeiseNoetig).toDouble();
        if (speiseNoetig == std::numeric_limits<double>::infinity())
        {
            return zucker;
        }
        else
        {
            double speiseVerfuegbar = data(idx.row(), ColSpeisemenge).toDouble();
            double potSpeise = BierCalc::wuerzeCO2Potential(sw, sreSchnellgaerprobe);
            double potZucker = BierCalc::zuckerCO2Potential();
            return zucker - speiseVerfuegbar * potSpeise / potZucker;
        }
    }
    case ColWoche:
    {
        Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
        if (status >= Brauhelfer::SudStatus::Abgefuellt)
        {
            QDateTime dt = bh->modelNachgaerverlauf()->getLastDateTime(data(idx.row(), ColID).toInt());
            if (!dt.isValid())
                dt = data(idx.row(), ColAbfuelldatum).toDateTime();
            if (dt.isValid())
                return dt.daysTo(QDateTime::currentDateTime()) / 7 + 1;
        }
        return 0;
    }
    case ColReifezeitDelta:
    {
        Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
        if (status >= Brauhelfer::SudStatus::Abgefuellt)
        {
            QDateTime dt = bh->modelNachgaerverlauf()->getLastDateTime(data(idx.row(), ColID).toInt());
            if (!dt.isValid())
                dt = data(idx.row(), ColAbfuelldatum).toDateTime();
            if (dt.isValid())
            {
                qint64 tageReifung = dt.daysTo(QDateTime::currentDateTime());
                int tageReifungSoll = data(idx.row(), ColReifezeit).toInt() * 7;
                return tageReifungSoll - tageReifung;
            }
        }
        return 0;
    }
    case ColAbfuellenBereitZutaten:
    {
        ProxyModel modelHefegaben;
        modelHefegaben.setSourceModel(bh->modelHefegaben());
        modelHefegaben.setFilterKeyColumn(bh->modelHefegaben()->ColSudID);
        modelHefegaben.setFilterRegExp(QString("^%1$").arg(data(idx.row(), ColID).toInt()));
        for (int r = 0; r < modelHefegaben.rowCount(); ++r)
        {
            if (!modelHefegaben.data(r, ModelHefegaben::ColAbfuellbereit).toBool())
                return false;
        }
        ProxyModel modelWeitereZutatenGaben;
        modelWeitereZutatenGaben.setSourceModel(bh->modelWeitereZutatenGaben());
        modelWeitereZutatenGaben.setFilterKeyColumn(bh->modelWeitereZutatenGaben()->ColSudID);
        modelWeitereZutatenGaben.setFilterRegExp(QString("^%1$").arg(data(idx.row(), ColID).toInt()));
        for (int r = 0; r < modelWeitereZutatenGaben.rowCount(); ++r)
        {
            if (!modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColAbfuellbereit).toBool())
                return false;
        }
        return true;
    }
    case ColMengeSollKochbeginn:
    {
        double mengeSollKochEnde = data(idx.row(), ColMengeSollKochende).toDouble();
        double kochdauer = data(idx.row(), ColKochdauerNachBitterhopfung).toDouble();
        double verdampfungsziffer = data(idx.row(), ColVerdampfungsrate).toDouble();
        return mengeSollKochEnde * (1 + (verdampfungsziffer * kochdauer / (60 * 100)));
    }
    case ColMengeSollKochende:
    {
        double mengeSoll = data(idx.row(), ColMenge).toDouble();
        double hgf = 1 + data(idx.row(), ColhighGravityFaktor).toInt() / 100.0;
        return mengeSoll / hgf;
    }
    case ColWuerzemengeAnstellenTotal:
    {
        return data(idx.row(), ColWuerzemengeAnstellen).toDouble() + data(idx.row(), ColSpeisemenge).toDouble();
    }
    case ColSW_Malz:
    {
        return data(idx.row(), ColSW).toDouble() - swWzMaischenRecipe[idx.row()] - swWzKochenRecipe[idx.row()] - swWzGaerungRecipe[idx.row()];
    }
    case ColSW_WZ_Maischen:
    {
        return swWzMaischenRecipe[idx.row()];
    }
    case ColSW_WZ_Kochen:
    {
        return swWzKochenRecipe[idx.row()];
    }
    case ColSW_WZ_Gaerung:
    {
        return swWzGaerungRecipe[idx.row()];
    }
    case ColSWSollKochbeginn:
    {
        double sw = data(idx.row(), ColSW).toDouble() - swWzKochenRecipe[idx.row()] - swWzGaerungRecipe[idx.row()];
        double hgf = 1 + data(idx.row(), ColhighGravityFaktor).toInt() / 100.0;
        double kochdauer = data(idx.row(), ColKochdauerNachBitterhopfung).toDouble();
        double verdampfungsziffer = data(idx.row(), ColVerdampfungsrate).toDouble();
        return sw * hgf / (1 + (verdampfungsziffer * kochdauer / (60 * 100)));
    }
    case ColSWSollKochbeginnMitWz:
    {
        double sw = data(idx.row(), ColSWSollKochende).toDouble();
        double kochdauer = data(idx.row(), ColKochdauerNachBitterhopfung).toDouble();
        double verdampfungsziffer = data(idx.row(), ColVerdampfungsrate).toDouble();
        return sw / (1 + (verdampfungsziffer * kochdauer / (60 * 100)));
    }
    case ColSWSollKochende:
    {
        double sw = data(idx.row(), ColSWSollAnstellen).toDouble();
        double hgf = 1 + data(idx.row(), ColhighGravityFaktor).toInt() / 100.0;
        return sw * hgf;
    }
    case ColSWSollAnstellen:
    {
        double sw = data(idx.row(), ColSW).toDouble();
        return sw - swWzGaerungRecipe[idx.row()];
    }
    case ColVerdampfungszifferIst:
    {
        double V1 = data(idx.row(), ColWuerzemengeKochbeginn).toDouble();
        double V2 = data(idx.row(), ColWuerzemengeVorHopfenseihen).toDouble();
        double t = data(idx.row(), ColKochdauerNachBitterhopfung).toDouble();
        return BierCalc::verdampfungsziffer(V1, V2, t);
    }
    case ColsEVG:
    {
        double sw = data(idx.row(), ColSWIst).toDouble();
        double sre = data(idx.row(), ColSREIst).toDouble();
        return BierCalc::vergaerungsgrad(sw, sre);
    }
    case ColtEVG:
    {
        double sw = data(idx.row(), ColSWIst).toDouble();
        double sre = data(idx.row(), ColSREIst).toDouble();
        double tre = BierCalc::toTRE(sw, sre);
        return BierCalc::vergaerungsgrad(sw, tre);
    }
    case ColAlkohol:
    {
        double sw = data(idx.row(), ColSW).toDouble();
        double vg = data(idx.row(), ColVergaerungsgrad).toDouble();
        double sre = BierCalc::sreAusVergaerungsgrad(sw, vg);
        return BierCalc::alkohol(sw, sre);
    }
    case ColAnlageVerdampfungsziffer:
    {
        return dataAnlage(idx.row(), ModelAusruestung::ColVerdampfungsziffer);
    }
    case ColAnlageSudhausausbeute:
    {
        return dataAnlage(idx.row(), ModelAusruestung::ColSudhausausbeute);
    }
    case ColRestalkalitaetFaktor:
    {
        double ist = bh->modelWasser()->getValueFromSameRow(ModelWasser::ColName, data(idx.row(), ColWasserprofil), ModelWasser::ColRestalkalitaet).toDouble();
        double soll = data(idx.row(), ColRestalkalitaetSoll).toDouble();
        double fac = (ist -  soll) * 0.033333333;
        if (fac < 0.0)
            fac = 0.0;
        return fac;
    }
    case ColFaktorHauptgussEmpfehlung:
    {
        double ebc = data(idx.row(), Colerg_Farbe).toDouble();
        if (ebc < 50)
            return 4.0 - ebc * 0.02;
        else
            return 3.0;
    }
    case ColBewertungMittel:
    {
        return bh->modelBewertungen()->mean(data(idx.row(), ColID));
    }
    default:
        return QVariant();
    }
}

bool ModelSud::setDataExt(const QModelIndex &idx, const QVariant &value)
{
    bool ret;
    mSkipUpdateOnOtherModelChanged = true;
    ret = setDataExt_impl(idx, value);
    mSkipUpdateOnOtherModelChanged = false;
    return ret;
}

bool ModelSud::setDataExt_impl(const QModelIndex &idx, const QVariant &value)
{
    switch(idx.column())
    {
    case ColBraudatum:
    {
        if (QSqlTableModel::setData(idx, value.toDateTime().toString(Qt::ISODate)))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status < Brauhelfer::SudStatus::Abgefuellt)
                setData(idx.row(), ColAbfuelldatum, value);
            return true;
        }
        return false;
    }
    case ColAbfuelldatum:
    {
        return QSqlTableModel::setData(idx, value.toDateTime().toString(Qt::ISODate));
    }
    case ColErstellt:
    {
        return QSqlTableModel::setData(idx, value.toDateTime().toString(Qt::ISODate));
    }
    case ColGespeichert:
    {
        return QSqlTableModel::setData(idx, value.toDateTime().toString(Qt::ISODate));
    }
    case ColAnlage:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
            {
                setData(idx.row(), ColSudhausausbeute, dataAnlage(idx.row(), ModelAusruestung::ColSudhausausbeute));
                setData(idx.row(), ColVerdampfungsrate, dataAnlage(idx.row(), ModelAusruestung::ColVerdampfungsziffer));
            }
            return true;
        }
        return false;
    }
    case ColMenge:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                setData(idx.row(), ColWuerzemengeKochbeginn, value);
            return true;
        }
        return false;
    }
    case ColWuerzemengeKochbeginn:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                setData(idx.row(), ColWuerzemengeVorHopfenseihen, value);
            return true;
        }
        return false;
    }
    case ColWuerzemengeVorHopfenseihen:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                setData(idx.row(), ColWuerzemengeKochende, value);
            return true;
        }
        return false;
    }
    case ColWuerzemengeKochende:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
            {
                double m = value.toDouble() + data(idx.row(), ColMenge).toDouble() - data(idx.row(), ColMengeSollKochende).toDouble();
                setData(idx.row(), ColWuerzemengeAnstellenTotal, m);
            }
            return true;
        }
        return false;
    }
    case ColWuerzemengeAnstellenTotal:
    {
        double v = value.toDouble() - data(idx.row(), ColSpeisemenge).toDouble();
        return setData(idx.row(), ColWuerzemengeAnstellen, v);
    }
    case ColWuerzemengeAnstellen:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                setData(idx.row(), ColJungbiermengeAbfuellen, value);
            return true;
        }
        return false;
    }
    case ColSpeisemenge:
    {
        double v = data(idx.row(), ColWuerzemengeAnstellenTotal).toDouble() - value.toDouble();
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                QSqlTableModel::setData(index(idx.row(), ColWuerzemengeAnstellen), v);
            return true;
        }
        return false;
    }
    case Colerg_AbgefuellteBiermenge:
    {
        double speise = data(idx.row(), ColSpeiseAnteil).toDouble() / 1000;
        double jungbiermenge = data(idx.row(), ColJungbiermengeAbfuellen).toDouble();
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status < Brauhelfer::SudStatus::Abgefuellt)
            {
                if (jungbiermenge > 0.0)
                    QSqlTableModel::setData(index(idx.row(), ColJungbiermengeAbfuellen), value.toDouble() / (1 + speise / jungbiermenge));
                else
                    QSqlTableModel::setData(index(idx.row(), ColJungbiermengeAbfuellen), value.toDouble() - speise);
            }
            return true;
        }
        return false;
    }
    case ColSW:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                setData(idx.row(), ColSWKochbeginn, value);
            return true;
        }
        return false;
    }
    case ColSWKochbeginn:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                setData(idx.row(), ColSWKochende, value);
            return true;
        }
        return false;
    }
    case ColSWKochende:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
                setData(idx.row(), ColSWAnstellen, value);
            return true;
        }
        return false;
    }
    case ColSWAnstellen:
    {
        if (QSqlTableModel::setData(idx, value))
        {
            Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(idx.row(), ColStatus).toInt());
            if (status == Brauhelfer::SudStatus::Rezept)
            {
                double vg = data(idx.row(), ColVergaerungsgrad).toDouble();
                double sre = BierCalc::sreAusVergaerungsgrad(value.toDouble(), vg);
                setData(idx.row(), ColSWSchnellgaerprobe, sre);
                setData(idx.row(), ColSWJungbier, sre);
            }
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

Qt::ItemFlags ModelSud::flags(const QModelIndex &idx) const
{
    Qt::ItemFlags itemFlags = SqlTableModel::flags(idx);
    if (idx.column() == ColWuerzemengeAnstellenTotal)
        itemFlags |= Qt::ItemIsEditable;
    return itemFlags;
}

QVariant ModelSud::dataSud(QVariant sudId, int col)
{
    return getValueFromSameRow(ModelSud::ColID, sudId, col);
}

QVariant ModelSud::dataAnlage(int row, int col) const
{
    return bh->modelAusruestung()->getValueFromSameRow(ModelAusruestung::ColName, data(row, ColAnlage), col);
}

QVariant ModelSud::dataWasser(int row, int col) const
{
    return bh->modelWasser()->getValueFromSameRow(ModelWasser::ColName, data(row, ColWasserprofil), col);
}

void ModelSud::update(int row)
{
    if (row < 0 || row >= rowCount())
        return;

    if (mUpdating)
        return;
    mUpdating = true;
    mSignalModifiedBlocked = true;

    double menge, sw;

    qDebug() << "ModelSud::update():" << data(row, ColID).toInt();

    updateSwWeitereZutaten(row);

    Brauhelfer::SudStatus status = static_cast<Brauhelfer::SudStatus>(data(row, ColStatus).toInt());
    if (status == Brauhelfer::SudStatus::Rezept)
    {
        // recipe
        double mengeRecipe = data(row, ColMenge).toDouble();
        double swRecipe = data(row, ColSW).toDouble();
        double hgf = 1 + data(row, ColhighGravityFaktor).toInt() / 100.0;

        // erg_S_Gesamt
        sw = swRecipe - swWzMaischenRecipe[row] - swWzKochenRecipe[row] - swWzGaerungRecipe[row];
        double ausb = data(row, ColSudhausausbeute).toDouble();
        double schuet = BierCalc::schuettung(sw * hgf, mengeRecipe / hgf, ausb, true);
        setData(row, Colerg_S_Gesamt, schuet);

        // erg_W_Gesamt, erg_WHauptguss, erg_WNachguss
        updateWasser(row);

        // erg_Farbe
        updateFarbe(row);

        // erg_Sudhausausbeute
        sw = data(row, ColSWKochende).toDouble() - swWzMaischenRecipe[row] - swWzKochenRecipe[row];
        menge = data(row, ColWuerzemengeVorHopfenseihen).toDouble();
        setData(row, Colerg_Sudhausausbeute, BierCalc::sudhausausbeute(sw, menge, schuet, true));

        // erg_EffektiveAusbeute
        sw = data(row, ColSWAnstellen).toDouble() * hgf - swWzMaischenRecipe[row] - swWzKochenRecipe[row];
        menge = data(row, ColWuerzemengeAnstellenTotal).toDouble() / hgf;
        setData(row, Colerg_EffektiveAusbeute, BierCalc::sudhausausbeute(sw , menge, schuet, true));
    }
    if (status <= Brauhelfer::SudStatus::Gebraut)
    {
        // erg_Alkohol
        double sre = data(row, ColSREIst).toDouble();
        sw = data(row, ColSWAnstellen).toDouble() + swWzGaerungCurrent[row];
        menge = data(row, ColWuerzemengeAnstellen).toDouble();
        if (menge > 0.0)
            sw += (data(row, ColZuckerAnteil).toDouble() / 10) / menge;
        setData(row, Colerg_Alkohol, BierCalc::alkohol(sw, sre));

        // erg_AbgefuellteBiermenge
        double jungbiermenge = data(row, ColJungbiermengeAbfuellen).toDouble();
        double speise = data(row, ColSpeiseAnteil).toDouble() / 1000;
        setData(row, Colerg_AbgefuellteBiermenge, jungbiermenge + speise);

        // erg_Preis
        updatePreis(row);
    }

    setData(row, ColGespeichert, QDateTime::currentDateTime());

    mSignalModifiedBlocked = false;
    mUpdating = false;

    emit modified();
}

void ModelSud::updateSwWeitereZutaten(int row)
{
    swWzMaischenRecipe[row] = 0.0;
    swWzKochenRecipe[row] = 0.0;
    swWzGaerungRecipe[row] = 0.0;
    swWzGaerungCurrent[row] = 0.0;

    ProxyModel modelWeitereZutatenGaben;
    modelWeitereZutatenGaben.setSourceModel(bh->modelWeitereZutatenGaben());
    modelWeitereZutatenGaben.setFilterKeyColumn(ModelWeitereZutatenGaben::ColSudID);
    modelWeitereZutatenGaben.setFilterRegExp(QRegExp(QString("^%1$").arg(data(row, ColID).toInt())));
    for (int r = 0; r < modelWeitereZutatenGaben.rowCount(); ++r)
    {
        Brauhelfer::ZusatzTyp typ = static_cast<Brauhelfer::ZusatzTyp>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColTyp).toInt());
        if (typ == Brauhelfer::ZusatzTyp::Hopfen)
            continue;
        double ausbeute = modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColAusbeute).toDouble();
        if (ausbeute > 0.0)
        {
            double menge = modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColMenge).toDouble();
            Brauhelfer::Einheit einheit = static_cast<Brauhelfer::Einheit>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColEinheit).toInt());
            if (einheit == Brauhelfer::Einheit::Stk)
                menge = qCeil(menge);
            else
                menge /= 1000;
            double sw = menge * ausbeute;
            Brauhelfer::ZusatzZeitpunkt zeitpunkt = static_cast<Brauhelfer::ZusatzZeitpunkt>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColZeitpunkt).toInt());
            Brauhelfer::ZusatzStatus status = static_cast<Brauhelfer::ZusatzStatus>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColZugabestatus).toInt());
            switch (zeitpunkt)
            {
            case Brauhelfer::ZusatzZeitpunkt::Gaerung:
                swWzGaerungRecipe[row] += sw ;
                if (status != Brauhelfer::ZusatzStatus::NichtZugegeben)
                    swWzGaerungCurrent[row] += sw;
                break;
            case Brauhelfer::ZusatzZeitpunkt::Kochen:
                swWzKochenRecipe[row] += sw;
                break;
            case Brauhelfer::ZusatzZeitpunkt::Maischen:
                swWzMaischenRecipe[row] += sw;
                break;
            }
        }
    }
}

void ModelSud::updateWasser(int row)
{
    double hg = 0.0, ng = 0.0;
    double schuet = data(row, Colerg_S_Gesamt).toDouble();
    double menge = data(row, ColMengeSollKochbeginn).toDouble();

    Brauhelfer::AnlageTyp anlageTyp = static_cast<Brauhelfer::AnlageTyp>(dataAnlage(row, ModelAusruestung::ColTyp).toInt());
    switch (anlageTyp)
    {
    case Brauhelfer::AnlageTyp::GrainfatherG30:
        hg = schuet * 2.7 + 3.5;
        if (schuet > 4.5)
            ng = menge - hg + schuet * 0.8;
        else
            ng = menge - hg + schuet * 0.8 - 2;
        break;
    default:
        hg = schuet * data(row, ColFaktorHauptguss).toDouble();
        ng = menge - hg + schuet * 0.96;
        break;
    }

    ng += dataAnlage(row, ModelAusruestung::ColKorrekturWasser).toDouble();

    setData(row, Colerg_WHauptguss, hg);
    setData(row, Colerg_WNachguss, ng);
    setData(row, Colerg_W_Gesamt, hg + ng);
}

void ModelSud::updateFarbe(int row)
{
    QRegExp sudReg = QRegExp(QString("^%1$").arg(data(row, ColID).toInt()));
    double ebc = 0.0;
    double d = 0.0;
    double gs = 0.0;

    ProxyModel modelMalzschuettung;
    modelMalzschuettung.setSourceModel(bh->modelMalzschuettung());
    modelMalzschuettung.setFilterKeyColumn(ModelMalzschuettung::ColSudID);
    modelMalzschuettung.setFilterRegExp(sudReg);
    for (int r = 0; r < modelMalzschuettung.rowCount(); ++r)
    {
        double farbe = modelMalzschuettung.data(r, ModelMalzschuettung::ColFarbe).toDouble();
        double menge = modelMalzschuettung.data(r, ModelMalzschuettung::Colerg_Menge).toDouble();
        d += menge * farbe;
        gs += menge;
    }

    ProxyModel modelWeitereZutatenGaben;
    modelWeitereZutatenGaben.setSourceModel(bh->modelWeitereZutatenGaben());
    modelWeitereZutatenGaben.setFilterKeyColumn(ModelWeitereZutatenGaben::ColSudID);
    modelWeitereZutatenGaben.setFilterRegExp(sudReg);
    for (int r = 0; r < modelWeitereZutatenGaben.rowCount(); ++r)
    {
        Brauhelfer::ZusatzTyp typ = static_cast<Brauhelfer::ZusatzTyp>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColTyp).toInt());
        if (typ == Brauhelfer::ZusatzTyp::Hopfen)
            continue;
        double farbe = modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColFarbe).toDouble();
        if (farbe > 0.0)
        {
            double menge = modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::Colerg_Menge).toDouble();
            Brauhelfer::Einheit einheit = static_cast<Brauhelfer::Einheit>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColEinheit).toInt());
            if (einheit == Brauhelfer::Einheit::Stk)
                menge = qCeil(menge);
            else
                menge /= 1000;
            d += menge * farbe;
            gs += menge;
        }
    }

    if (gs > 0.0)
    {
        double sw = data(row, ColSW).toDouble() - swWzKochenRecipe[row] - swWzGaerungRecipe[row];
        ebc = (d / gs) * sw / 10 + 2;
        ebc += dataAnlage(row, ModelAusruestung::ColKorrekturFarbe).toDouble();
    }
    setData(row, Colerg_Farbe, ebc);
}

void ModelSud::updatePreis(int row)
{
    QRegExp sudReg = QRegExp(QString("^%1$").arg(data(row, ColID).toInt()));
    double summe = 0.0;

    double kostenSchuettung = 0.0;
    ProxyModel modelMalzschuettung;
    modelMalzschuettung.setSourceModel(bh->modelMalzschuettung());
    modelMalzschuettung.setFilterKeyColumn(ModelMalzschuettung::ColSudID);
    modelMalzschuettung.setFilterRegExp(sudReg);
    for (int r = 0; r < modelMalzschuettung.rowCount(); ++r)
    {
        QVariant name = modelMalzschuettung.data(r, ModelMalzschuettung::ColName);
        double menge = modelMalzschuettung.data(r, ModelMalzschuettung::Colerg_Menge).toDouble();
        double preis = bh->modelMalz()->getValueFromSameRow(ModelMalz::ColBeschreibung, name, ModelMalz::ColPreis).toDouble();
        kostenSchuettung += preis * menge;
    }
    summe += kostenSchuettung;

    double kostenHopfen = 0.0;
    ProxyModel modelHopfengaben;
    modelHopfengaben.setSourceModel(bh->modelHopfengaben());
    modelHopfengaben.setFilterKeyColumn(ModelHopfengaben::ColSudID);
    modelHopfengaben.setFilterRegExp(sudReg);
    for (int r = 0; r < modelHopfengaben.rowCount(); ++r)
    {
        QVariant name = modelHopfengaben.data(r, ModelHopfengaben::ColName);
        double menge = modelHopfengaben.data(r, ModelHopfengaben::Colerg_Menge).toDouble();
        double preis = bh->modelHopfen()->getValueFromSameRow(ModelHopfen::ColBeschreibung, name, ModelHopfen::ColPreis).toDouble();
        kostenHopfen += preis * menge / 1000;
    }
    summe += kostenHopfen;

    double kostenHefe = 0.0;
    ProxyModel modelHefegaben;
    modelHefegaben.setSourceModel(bh->modelHefegaben());
    modelHefegaben.setFilterKeyColumn(ModelHefegaben::ColSudID);
    modelHefegaben.setFilterRegExp(sudReg);
    for (int r = 0; r < modelHefegaben.rowCount(); ++r)
    {
        QVariant name = modelHefegaben.data(r, ModelHefegaben::ColName);
        int menge = modelHefegaben.data(r, ModelHefegaben::ColMenge).toInt();
        double preis = bh->modelHefe()->getValueFromSameRow(ModelHefe::ColBeschreibung, name, ModelHefe::ColPreis).toDouble();
        kostenHefe += preis * menge;
    }
    summe += kostenHefe;

    double kostenWeitereZutaten = 0.0;
    ProxyModel modelWeitereZutatenGaben;
    modelWeitereZutatenGaben.setSourceModel(bh->modelWeitereZutatenGaben());
    modelWeitereZutatenGaben.setFilterKeyColumn(ModelWeitereZutatenGaben::ColSudID);
    modelWeitereZutatenGaben.setFilterRegExp(sudReg);
    for (int r = 0; r < modelWeitereZutatenGaben.rowCount(); ++r)
    {
        QVariant name = modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColName);
        double menge = modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::Colerg_Menge).toDouble();
        double preis;
        Brauhelfer::ZusatzTyp typ = static_cast<Brauhelfer::ZusatzTyp>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColTyp).toInt());
        if (typ == Brauhelfer::ZusatzTyp::Hopfen)
        {
            preis = bh->modelHopfen()->getValueFromSameRow(ModelHopfen::ColBeschreibung, name, ModelHopfen::ColPreis).toDouble();
            preis /= 1000;
        }
        else
        {
            preis = bh->modelWeitereZutaten()->getValueFromSameRow(ModelWeitereZutaten::ColBeschreibung, name, ModelWeitereZutaten::ColPreis).toDouble();
            Brauhelfer::Einheit einheit = static_cast<Brauhelfer::Einheit>(modelWeitereZutatenGaben.data(r, ModelWeitereZutatenGaben::ColEinheit).toInt());
            if (einheit == Brauhelfer::Einheit::Stk)
                menge = qCeil(menge);
            else
                preis /= 1000;
        }
        kostenWeitereZutaten += preis * menge;
    }
    summe += kostenWeitereZutaten;

    double kostenSonstiges = data(row, ColKostenWasserStrom).toDouble();
    summe += kostenSonstiges;

    double kostenAnlage = dataAnlage(row, ModelAusruestung::ColKosten).toDouble();
    summe += kostenAnlage;

    setData(row, Colerg_Preis, summe / data(row, ColMengeIst).toDouble());
}

bool ModelSud::removeRows(int row, int count, const QModelIndex &parent)
{
    QList<int> sudIds;
    for (int i = 0; i < count; ++i)
        sudIds.append(data(row + i, ColID).toInt());
    if (SqlTableModel::removeRows(row, count, parent))
    {
        removeRowsFrom(bh->modelRasten(), ModelRasten::ColSudID, sudIds);
        removeRowsFrom(bh->modelMalzschuettung(), ModelMalzschuettung::ColSudID, sudIds);
        removeRowsFrom(bh->modelHopfengaben(), ModelHopfengaben::ColSudID, sudIds);
        removeRowsFrom(bh->modelWeitereZutatenGaben(), ModelWeitereZutatenGaben::ColSudID, sudIds);
        removeRowsFrom(bh->modelHefegaben(), ModelHefegaben::ColSudID, sudIds);
        removeRowsFrom(bh->modelSchnellgaerverlauf(), ModelSchnellgaerverlauf::ColSudID, sudIds);
        removeRowsFrom(bh->modelHauptgaerverlauf(), ModelHauptgaerverlauf::ColSudID, sudIds);
        removeRowsFrom(bh->modelNachgaerverlauf(), ModelNachgaerverlauf::ColSudID, sudIds);
        removeRowsFrom(bh->modelBewertungen(), ModelBewertungen::ColSudID, sudIds);
        removeRowsFrom(bh->modelAnhang(), ModelAnhang::ColSudID, sudIds);
        removeRowsFrom(bh->modelEtiketten(), ModelEtiketten::ColSudID, sudIds);
        removeRowsFrom(bh->modelTags(), ModelTags::ColSudID, sudIds);
        return true;
    }
    return false;
}

void ModelSud::removeRowsFrom(SqlTableModel* model, int colId, const QList<int> &sudIds)
{
    for (int r = 0; r < model->rowCount(); ++r)
    {
        if (sudIds.contains(model->data(r, colId).toInt()))
        {
            model->removeRows(r);
            r--;
        }
    }
}

void ModelSud::defaultValues(QMap<int, QVariant> &values) const
{
    if (!values.contains(ColID))
        values.insert(ColID, getNextId());
    if (!values.contains(ColErstellt))
        values.insert(ColErstellt, QDateTime::currentDateTime());
    if (!values.contains(ColSudname))
        values.insert(ColSudname, "Sudname");
    if (!values.contains(ColSudnummer))
        values.insert(ColSudnummer, 0);
    if (!values.contains(ColAnlage) && bh->modelAusruestung()->rowCount() == 1)
        values.insert(ColAnlage, bh->modelAusruestung()->data(0, ModelAusruestung::ColName));
    if (!values.contains(ColWasserprofil) && bh->modelWasser()->rowCount() == 1)
        values.insert(ColWasserprofil, bh->modelWasser()->data(0, ModelWasser::ColName));
    if (!values.contains(ColMenge))
        values.insert(ColMenge, 20);
    if (!values.contains(ColSW))
        values.insert(ColSW, 12);
    if (!values.contains(ColCO2))
        values.insert(ColCO2, 5);
    if (!values.contains(ColIBU))
        values.insert(ColIBU, 26);
    if (!values.contains(ColKochdauerNachBitterhopfung))
        values.insert(ColKochdauerNachBitterhopfung, 60);
    if (!values.contains(ColberechnungsArtHopfen))
        values.insert(ColberechnungsArtHopfen, static_cast<int>(Brauhelfer::BerechnungsartHopfen::IBU));
    if (!values.contains(ColVergaerungsgrad))
        values.insert(ColVergaerungsgrad, 70);
    if (!values.contains(ColTemperaturJungbier))
        values.insert(ColTemperaturJungbier, 20.0);
    if (!values.contains(ColStatus))
        values.insert(ColStatus, static_cast<int>(Brauhelfer::SudStatus::Rezept));
}

QMap<int, QVariant> ModelSud::copyValues(int row) const
{
    QMap<int, QVariant> values = SqlTableModel::copyValues(row);
    values[ColID] = getNextId();
    return values;
}
