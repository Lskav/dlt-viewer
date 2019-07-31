#include <QProgressDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>

#include "dltexporter.h"
#include "fieldnames.h"
#include "project.h"

DltExporter::DltExporter(QObject *parent) :
    QObject(parent)
{
    size = 0;
    from = NULL;
    to = NULL;
    pluginManager = NULL;
    selection = NULL;
    exportFormat = FormatDlt;
    exportSelection = SelectionAll;
}

QString DltExporter::escapeCSVValue(QString arg)
{
    QString retval = arg.replace(QChar('\"'), QString("\"\""));
    retval = QString("\"%1\"").arg(retval);
    return retval;
}

bool DltExporter::writeCSVHeader(QFile *file)
{
    QString header("\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"%6\",\"%7\",\"%8\",\"%9\",\"%10\",\"%11\",\"%12\",\"%13\"\n");
    header = header.arg(FieldNames::getName(FieldNames::Index))
                    .arg(FieldNames::getName(FieldNames::Time))
                    .arg(FieldNames::getName(FieldNames::TimeStamp))
                    .arg(FieldNames::getName(FieldNames::Counter))
                    .arg(FieldNames::getName(FieldNames::EcuId))
                    .arg(FieldNames::getName(FieldNames::AppId))
                    .arg(FieldNames::getName(FieldNames::ContextId))
                    .arg(FieldNames::getName(FieldNames::SessionId))
                    .arg(FieldNames::getName(FieldNames::Type))
                    .arg(FieldNames::getName(FieldNames::Subtype))
                    .arg(FieldNames::getName(FieldNames::Mode))
                    .arg(FieldNames::getName(FieldNames::ArgCount))
                    .arg(FieldNames::getName(FieldNames::Payload));
    return file->write(header.toLatin1().constData()) < 0 ? false : true;
}

void DltExporter::writeCSVLine(int index, QFile *to, QDltMsg msg)
{
    QString text("");

    text += escapeCSVValue(QString("%1").arg(index)).append(",");
    text += escapeCSVValue(QString("%1.%2").arg(msg.getTimeString()).arg(msg.getMicroseconds(),6,10,QLatin1Char('0'))).append(",");
    text += escapeCSVValue(QString("%1.%2").arg(msg.getTimestamp()/10000).arg(msg.getTimestamp()%10000,4,10,QLatin1Char('0'))).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getMessageCounter())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getEcuid())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getApid())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getCtid())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getSessionid())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getTypeString())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getSubtypeString())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getModeString())).append(",");
    text += escapeCSVValue(QString("%1").arg(msg.getNumberOfArguments())).append(",");
    text += escapeCSVValue(msg.toStringPayload().simplified());
    text += "\n";

    to->write(text.toLatin1().constData());
}

bool DltExporter::start()
{
    /* Sort the selection list and create Row list */
    if(exportSelection == DltExporter::SelectionSelected && selection != NULL)
    {
        qSort(selection->begin(), selection->end());
        selectedRows.clear();
        for(int num=0;num<selection->count();num++)
        {
            QModelIndex index = selection->at(num);
            if(index.column() == 0)
                selectedRows.append(index.row());
        }
    }

    /* open the export file */
    if(exportFormat == DltExporter::FormatAscii ||
       exportFormat == DltExporter::FormatUTF8 ||
       exportFormat == DltExporter::FormatCsv)
    {
        if(!to->open(QIODevice::WriteOnly | QIODevice::Text))
        {
            if ( true == QDltOptManager::getInstance()->issilentMode() )
             {
             qDebug() << QString("ERROR - cannot open the export file %1").arg(to->fileName());
             }
            else
            QMessageBox::critical(qobject_cast<QWidget *>(parent()), QString("DLT Viewer"),
                                  QString("Cannot open the export file %1").arg(to->fileName()));
            return false;
        }
    }
    else if((exportFormat == DltExporter::FormatDlt)||(exportFormat == DltExporter::FormatDltDecoded))
    {
        if(!to->open(QIODevice::WriteOnly))
        {
            if ( true == QDltOptManager::getInstance()->issilentMode() )
             {
             qDebug() << QString("ERROR - cannot open the export file %1").arg(to->fileName());
             }
            else
            QMessageBox::critical(qobject_cast<QWidget *>(parent()), QString("DLT Viewer"),
                                  QString("Cannot open the export file %1").arg(to->fileName()));
            return false;
        }
    }

    /* write CSV header if CSV export */
    if(exportFormat == DltExporter::FormatCsv)
    {
        /* Write the first line of CSV file */
        if(!writeCSVHeader(to))
        {
            if ( true == QDltOptManager::getInstance()->issilentMode() )
             {
             qDebug() << QString("ERROR - cannot open the export file %1").arg(to->fileName());
             }
            else
            QMessageBox::critical(qobject_cast<QWidget *>(parent()), QString("DLT Viewer"),
                                  QString("Cannot open the export file %1").arg(to->fileName()));
            return false;
        }
    }

    /* calculate size */
    if(exportSelection == DltExporter::SelectionAll)
        size = from->size();
    else if(exportSelection == DltExporter::SelectionFiltered)
        size = from->sizeFilter();
    else if(exportSelection == DltExporter::SelectionSelected)
        size = selectedRows.size();
    else
        return false;

    /* success */
    return true;
}

bool DltExporter::finish()
{

    if(exportFormat == DltExporter::FormatAscii ||
       exportFormat == DltExporter::FormatUTF8 ||
       exportFormat == DltExporter::FormatCsv ||
       exportFormat == DltExporter::FormatDlt ||
       exportFormat == DltExporter::FormatDltDecoded)
    {
        /* close output file */
        to->close();
    }
    else if (exportFormat == DltExporter::FormatClipboard ||
             exportFormat == DltExporter::FormatClipboardPayloadOnly)
    {
        /* export to clipboard */
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(clipboardString);
    }

    return true;
}

bool DltExporter::getMsg(int num,QDltMsg &msg,QByteArray &buf)
{
    buf.clear();
    if(exportSelection == DltExporter::SelectionAll)
    {
        buf = from->getMsg(num);
    }
    else if(exportSelection == DltExporter::SelectionFiltered)
    {
        buf = from->getMsgFilter(num);
    }
    else if(exportSelection == DltExporter::SelectionSelected)
    {
        buf = from->getMsgFilter(selectedRows[num]);
    }
    else
    {
        qDebug() << "Unhandled error in" << __FILE__ << __LINE__;
        return false;
    }

    if( true == buf.isEmpty())
    {
        qDebug() << "Buffer empty in" << __FILE__ << __LINE__;
        return false;
    }
    return msg.setMsg(buf);
}

bool DltExporter::exportMsg(int num, QDltMsg &msg, QByteArray &buf)
{
    if((exportFormat == DltExporter::FormatDlt)||(exportFormat == DltExporter::FormatDltDecoded))
    {
        to->write(buf);
    }
    else if(exportFormat == DltExporter::FormatAscii ||
            exportFormat == DltExporter::FormatUTF8  ||
            exportFormat == DltExporter::FormatClipboard ||
            exportFormat == DltExporter::FormatClipboardPayloadOnly)
    {
        QString text;

        /* get message ASCII text */
        if(exportFormat != DltExporter::FormatClipboardPayloadOnly)
        {
            if(exportSelection == DltExporter::SelectionAll)
                text += QString("%1 ").arg(num);
            else if(exportSelection == DltExporter::SelectionFiltered)
                text += QString("%1 ").arg(from->getMsgFilterPos(num));
            else if(exportSelection == DltExporter::SelectionSelected)
                text += QString("%1 ").arg(from->getMsgFilterPos(selectedRows[num]));
            else
                return false;
            text += msg.toStringHeader();
            text += " ";
        }
        text += msg.toStringPayload().simplified();
        text += "\n";
        try
         {
            if(exportFormat == DltExporter::FormatAscii)
                /* write to file */
                to->write(text.toLatin1().constData());
            else if (exportFormat == DltExporter::FormatUTF8)
                to->write(text.toUtf8().constData());
            else if(exportFormat == DltExporter::FormatClipboard ||
                    exportFormat == DltExporter::FormatClipboardPayloadOnly)
                clipboardString += text;
         }
        catch (...)
         {
         }
    }
    else if(exportFormat == DltExporter::FormatCsv)
    {
        if(exportSelection == DltExporter::SelectionAll)
            writeCSVLine(num, to, msg);
        else if(exportSelection == DltExporter::SelectionFiltered)
            writeCSVLine(from->getMsgFilterPos(num), to, msg);
        else if(exportSelection == DltExporter::SelectionSelected)
            writeCSVLine(from->getMsgFilterPos(selectedRows[num]), to, msg);
        else
            return false;
    }

    return true;
}

void DltExporter::exportMessages(QDltFile *from, QFile *to, QDltPluginManager *pluginManager,
                         DltExporter::DltExportFormat exportFormat, DltExporter::DltExportSelection exportSelection, QModelIndexList *selection)
{
    QDltMsg msg;
    QByteArray buf;

    /* initialise values */
    int readErrors=0;
    int exportErrors=0;
    int exportCounter=0;
    int startFinishError=0;

    this->size = 0;
    this->from = from;
    this->to = to;
    clipboardString.clear();
    this->pluginManager = pluginManager;
    this->selection = selection;
    this->exportFormat = exportFormat;
    this->exportSelection = exportSelection;

    /* start export */
    if(!start())
    {
        qDebug() << "DLT Export start() failed";
        startFinishError++;
        return;
    }


    bool silentMode = !QDltOptManager::getInstance()->issilentMode();

    qDebug() << "Start DLT export of" << size << "messages" << "silent mode" << !silentMode;

    /* init fileprogress */

    QProgressDialog fileprogress("Export ...", "Cancel", 0, this->size, qobject_cast<QWidget *>(parent()));
    if (silentMode == true)
     {
      fileprogress.setWindowTitle("DLT Viewer");
      fileprogress.setWindowModality(Qt::WindowModal);
      fileprogress.show();
     }

    for(int num = 0;num<size;num++)
    {
        // Update progress dialog every 1000 lines
        if( 0 == (num%1000))
        {
          if (silentMode == true)
             {
              fileprogress.setValue(num);
             }
        }

        // get message
        if(false == getMsg(num,msg,buf))
        {
        //  finish();
        qDebug() << "DLT Export getMsg failed on msg index" << num;
        readErrors++;
        continue;
        //  return;
        }

        // decode message if needed
        if(exportFormat != DltExporter::FormatDlt)
        {
            pluginManager->decodeMsg(msg,silentMode);
            if (exportFormat == DltExporter::FormatDltDecoded)
            {
                msg.setNumberOfArguments(msg.sizeArguments());
                msg.getMsg(buf,true);
            }
        }

        // export message
        if(!exportMsg(num,msg,buf))
        {
            // finish();
          qDebug() << "DLT Export exportMsg() failed";
          exportErrors++;
          continue;
        }
     else
        exportCounter++;
    } // for loop
    if (silentMode == true)
     fileprogress.close();


    if (!finish())
    {
        startFinishError++;
    }


    if ( startFinishError>0 || readErrors>0 || exportErrors>0 )
    {
       qDebug() << "DLT Export finish() failed";
       if (silentMode == true ) // reversed login in this case !
       {
        QMessageBox::warning(NULL,"Export Errors!",QString("Exported successful: %1 / %2\n\nReadErrors:%3\nWriteErrors:%4\nStart/Finish errors:%5").arg(exportCounter).arg(size).arg(readErrors).arg(exportErrors).arg(startFinishError));
       }
       return;
    }
    qDebug() << "DLT export done for" << exportCounter << "messages with result" << startFinishError;
}
