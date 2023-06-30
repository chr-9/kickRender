#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "sequenceParser/Sequence.hpp"
#include "sequenceParser/filesystem.hpp"

using namespace std;

int minFrame = 0;
int maxFrame  = 0;
QString texPath = "";
QString PathReplaceFrom = "";
QString PathReplaceTo = "";
QString ass_texture_searchpath = "";

QString kickPath = "";
bool disableRenderWindow = true;
bool disableProgressiveRender = true;
bool skipLicenseCheck = false;

sequenceParser::Sequence seqInput;
std::string parentDir;
bool stopRender = false;
chrono::system_clock::time_point  c_start;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->pushButton_StopRender->setEnabled(false);
    ui->pushButton_StopRender->setVisible(false);

    QObject::connect( &m_proc, SIGNAL(readyReadStandardOutput()), this, SLOT(updateOutput()) );
    QObject::connect( &m_proc, SIGNAL(readyReadStandardError()), this, SLOT(updateError()) );
    QObject::connect( &m_proc, SIGNAL(finished(int, QProcess::ExitStatus)),
        this, SLOT(proc_finished(int, QProcess::ExitStatus)) );

    QSettings settings(QCoreApplication::applicationDirPath()+"/settings.ini", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    settings.beginGroup("Kick");
    kickPath = settings.value("path", "").toString();
    disableRenderWindow = settings.value("disableRenderWindow", true).toBool();
    disableProgressiveRender = settings.value("disableProgressiveRender", true).toBool();
    settings.endGroup();
    ui->lineEdit_KickPath->setText(kickPath);
    ui->checkBox->setChecked(disableRenderWindow);
    ui->checkBox_2->setChecked(disableProgressiveRender);

    if ( !kickPath.isEmpty()){
        if(QFile::exists(kickPath)){
            QStringList strlArgs;
            //strlArgs << "-nostdin" << "-av";
            strlArgs.append("-nostdin");
            strlArgs.append("-av");
            m_proc.start( kickPath, strlArgs );
            m_proc.waitForFinished();
            m_proc.close();
        }else{
            AppendLog("kick not found!");
        }
    }else{
        QFileDialog::Options options;
        QString strSelectedFilter;
        QString strFName = QFileDialog::getOpenFileName(
                this,
                tr( "Select kick" ),
                ".",
                tr( "kick*" ),
                &strSelectedFilter, options );

        if ( !strFName.isEmpty() )
        {
            kickPath = strFName;
            ui->lineEdit_KickPath->setText(kickPath);

            QStringList strlArgs;
            //strlArgs << "-nostdin" << "-av";
            strlArgs.append("-nostdin");
            strlArgs.append("-av");
            m_proc.start( kickPath, strlArgs );
            m_proc.waitForFinished();
            m_proc.close();
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

////////////////////////////////////////////
/// Core funcions
///

void MainWindow::on_pushButton_load_clicked()
{
    minFrame = 0;
    maxFrame  = 0;
    texPath = "";
    PathReplaceFrom = "";
    PathReplaceTo = "";
    ass_texture_searchpath = "";
    parentDir = "";

    QFileDialog::Options options;
    QString strSelectedFilter;
    QString strFName = QFileDialog::getOpenFileName(
            this,
            tr( "Select ass scene" ),
            "/mnt/raid/cache/kicktest",
            tr( "*.ass" ),
            &strSelectedFilter, options );

    if ( !strFName.isEmpty() )
    {
        // detect sequence
        QFileInfo fi(strFName);
        QDir d = fi.absoluteDir();
        parentDir = d.absolutePath().toStdString() + "/";

        sequenceParser::Item item;

        vector<string> filters = { "*.ass" };
        vector<sequenceParser::Item> items = sequenceParser::browse(parentDir, sequenceParser::eDetectionDefault, filters);
        if( !items.empty() ) {
            item = items.front();
        }else{
            return;
        }

        seqInput = item.getSequence();

        minFrame  = seqInput.getFirstTime();
        maxFrame  = seqInput.getLastTime();

        ui->label_seqPath->setText(QString::fromStdString(item.getAbsoluteFilepath()));
        ui->label_startFrame->setText(QString::number(minFrame)+"F");
        ui->label_endFrame->setText(QString::number(maxFrame)+"F");
        ui->label_totalFrame->setText(QString::number(maxFrame-minFrame)+"F");

        // read output exr paths
        AppendLog("Reading arnold scene source..\n");

        bool inner_driver_exr = false;
        bool inner_options = false;

        QFile file(strFName);
        file.open(QIODevice::ReadOnly);
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString linestr = in.readLine();
            if(linestr.contains("driver_exr", Qt::CaseInsensitive)){
                inner_driver_exr = true;
            }
            if(linestr.contains("options", Qt::CaseInsensitive)){
                inner_options = true;
            }

            if(inner_driver_exr){
                AppendLog(linestr);

                if(linestr.contains("filename", Qt::CaseInsensitive)){
                    QString outfilename = linestr;
                    outfilename.replace(" filename ", "");
                    outfilename.replace("\"", "");

                    if(outfilename.contains("\\")){
                        outfilename = texPath.mid(0, texPath.lastIndexOf("\\"));
                        PathReplaceFrom = outfilename + "\\";
                    }else{
                        outfilename = texPath.mid(0, texPath.lastIndexOf("/"));
                        PathReplaceFrom = outfilename + "/";
                    }
                }
                if(linestr.contains("}", Qt::CaseInsensitive)){
                    inner_driver_exr = false;
                    break;
                }
            }

            if(inner_options){
                if(linestr.contains("texture_searchpath", Qt::CaseInsensitive)){
                    ass_texture_searchpath = linestr;

                    texPath = linestr;
                    texPath.replace(" texture_searchpath ", "");
                    texPath.replace("\"", "");

                    texPath = texPath.mid(0, texPath.indexOf(";"));
                }
                if(linestr.contains("}", Qt::CaseInsensitive)){
                    inner_options = false;
                }
            }
        }
        file.close();

        if ( PathReplaceFrom.isEmpty() ){
            AppendLog("[Error]driver_exr not found");
            QMessageBox::warning( this, tr("Error"), tr("Unable to read the output path.\nPlease make sure that you have set driver_exr."));

            ui->label_seqPath->setText("-");
            minFrame = 0;
            maxFrame = 0;
            texPath="";
            ui->label_startFrame->setText("- F");
            ui->label_endFrame->setText("- F");
            ui->label_totalFrame->setText("- F");
            ui->lineEdit_TexPath->setText(texPath);
            ui->label_OutPathFrom->setText(PathReplaceFrom);
            ui->lineEdit_OutPathTo->setText(PathReplaceFrom);
            ui->pushButton_Render->setEnabled(false);
            return;
        }

        ui->lineEdit_TexPath->setText(texPath);
        ui->label_OutPathFrom->setText(PathReplaceFrom);
        PathReplaceTo = PathReplaceFrom;
        ui->lineEdit_OutPathTo->setText(PathReplaceTo);

        ui->pushButton_Render->setEnabled(true);
    }
}


void work(QString work_kickPath, QStringList work_strlArgs)
{
    //QThread::sleep(6);
    QProcess process;
    process.start( work_kickPath, work_strlArgs );
    process.waitForFinished(-1);
    //m_proc.close();
}

void MainWindow::on_pushButton_Render_clicked()
{
    texPath = ui->lineEdit_TexPath->text();
    PathReplaceTo = ui->lineEdit_OutPathTo->text();

    ui->pushButton_load->setEnabled(false);
    ui->lineEdit_TexPath->setEnabled(false);
    ui->pushButton_selectTexPath->setEnabled(false);
    ui->lineEdit_OutPathTo->setEnabled(false);
    ui->pushButton_selectOutPathTo->setEnabled(false);
    ui->checkBox->setEnabled(false);
    ui->checkBox_2->setEnabled(false);
    ui->checkBox_SkipLicenseCheck->setEnabled(false);
    ui->lineEdit_KickPath->setEnabled(false);
    ui->pushButton_selectKickPath->setEnabled(false);

    ui->pushButton_Render->setEnabled(false);
    ui->pushButton_Render->setVisible(false);
    ui->pushButton_StopRender->setEnabled(true);
    ui->pushButton_StopRender->setVisible(true);

    for(string filename : seqInput.getFiles()){
        if(stopRender){
            stopRender = false;
            break;
        }

        QFile file(QString::fromStdString(parentDir+filename));
        file.open(QIODevice::ReadOnly);
        QTextStream in(&file);
        QString assString = in.readAll();
        file.close();
        qApp->processEvents();

        AppendLog("Modifying arnold scene source..");
        assString.replace(ass_texture_searchpath, " texture_searchpath \""+texPath+"\"");
        assString.replace(PathReplaceFrom, PathReplaceTo);

        AppendLog("Saving temp.ass..");
        QFile filewrite(QCoreApplication::applicationDirPath()+"/temp.ass");
        if (!filewrite.open(QIODevice::WriteOnly)) {
            AppendLog("[Error]Can't save temp.ass");
            break;
        } else {
            QTextStream stream(&filewrite);
            stream << assString;
            stream.flush();
            filewrite.close();
        }

        //generate commands
        QStringList strlArgs;
        //strlArgs.append("-set options.texture_searchpath " + texPath);
        strlArgs.append("-nostdin");
        strlArgs.append("-nokeypress");
        if(disableRenderWindow)
            strlArgs.append("-dw");
        if(disableProgressiveRender)
            strlArgs.append("-dp");
        if(skipLicenseCheck){
            strlArgs.append("-sl");
        }

        strlArgs.append(QCoreApplication::applicationDirPath()+"/temp.ass");

        //run
        AppendLog("Executing kick with " + QString::fromStdString(filename));
        c_start = chrono::system_clock::now();
        QFuture<void> future = QtConcurrent::run(work, QString(kickPath), QStringList(strlArgs));
        QFutureWatcher<void> watcher;
        QEventLoop loop;
        connect(&watcher, SIGNAL(finished()), &loop, SLOT(quit()),  Qt::QueuedConnection);
        watcher.setFuture(future);
        AppendLog("Rendering..");
        loop.exec();

        int elapsed = chrono::duration_cast<std::chrono::seconds>(chrono::system_clock::now()-c_start).count();
        AppendLog("Render finished in "+sectoqstr(elapsed));

        if(stopRender){
            stopRender = false;
            break;
        }
    }

    AppendLog("Job Finished!");
    ui->pushButton_Render->setEnabled(true);
    ui->pushButton_Render->setVisible(true);
    ui->pushButton_StopRender->setEnabled(false);
    ui->pushButton_StopRender->setVisible(false);
    ui->pushButton_StopRender->setText("Stop");

    ui->pushButton_load->setEnabled(true);
    ui->lineEdit_TexPath->setEnabled(true);
    ui->pushButton_selectTexPath->setEnabled(true);
    ui->lineEdit_OutPathTo->setEnabled(true);
    ui->pushButton_selectOutPathTo->setEnabled(true);
    ui->checkBox->setEnabled(true);
    ui->checkBox_2->setEnabled(true);
    ui->checkBox_SkipLicenseCheck->setEnabled(true);
    ui->lineEdit_KickPath->setEnabled(true);
    ui->pushButton_selectKickPath->setEnabled(true);
}

void MainWindow::on_pushButton_StopRender_clicked()
{
    stopRender = true;
    ui->pushButton_Render->setEnabled(false);
    ui->pushButton_Render->setVisible(false);
    ui->pushButton_StopRender->setEnabled(false);
    ui->pushButton_StopRender->setText("Stopping arnold..");
}

QString MainWindow::sectoqstr(int sec){
    int h = sec / 3600;
    int m = (sec%3600) / 60;
    int s = sec % 60;

    QString timeString = QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return timeString;
}

////////////////////////////////////////////
/// UI funcions
///

void MainWindow::on_pushButton_selectTexPath_clicked()
{
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString strDir = QFileDialog::getExistingDirectory(
            this,
            tr("Choose Texture directory"),
            tr(""), options);

    if ( !strDir.isEmpty() )
    {
        texPath = strDir;
        ui->lineEdit_TexPath->setText(texPath);
    }
}

void MainWindow::on_pushButton_selectOutPathTo_clicked()
{
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString strDir = QFileDialog::getExistingDirectory(
            this,
            tr("Choose Output directory"),
            tr(""), options);

    if ( !strDir.isEmpty() )
    {
        PathReplaceTo = strDir+"/";
        ui->lineEdit_OutPathTo->setText(PathReplaceTo);
    }
}

void MainWindow::on_pushButton_selectKickPath_clicked()
{
    QFileDialog::Options options;
    QString strSelectedFilter;
    QString strFName = QFileDialog::getOpenFileName(
            this,
            tr( "Select kick" ),
            ".",
            tr( "kick*" ),
            &strSelectedFilter, options );

    if ( !strFName.isEmpty() )
    {
        kickPath = strFName;
        ui->lineEdit_KickPath->setText(kickPath);
    }
}


void MainWindow::on_checkBox_stateChanged()
{
    disableRenderWindow = ui->checkBox->isChecked();
}

void MainWindow::on_checkBox_2_stateChanged()
{
    disableProgressiveRender = ui->checkBox_2->isChecked();
}

void MainWindow::on_checkBox_SkipLicenseCheck_stateChanged()
{
    skipLicenseCheck =ui->checkBox_SkipLicenseCheck->isChecked();
}

void MainWindow::AppendLog(QString str)
{
    //LogText.append(str+"\n");
    //ui->plainTextEdit_log->setPlainText(LogText);
    ui->plainTextEdit_log->appendPlainText(str);
}


////////////////////////////////////////////
/// Other funcions
///

void MainWindow::updateOutput()
{
    QByteArray output = m_proc.readAllStandardOutput();
    QString str = QString::fromLocal8Bit( output );
    AppendLog("[kick]"+str);
}

void MainWindow::updateError()
{
    QByteArray output = m_proc.readAllStandardError();
    QString str = QString::fromLocal8Bit( output );
    AppendLog("[kick_Error]"+str);
}

void MainWindow::proc_finished( int exitCode, QProcess::ExitStatus exitStatus)
{
    if ( exitStatus == QProcess::CrashExit )
    {
        //QMessageBox::warning( this, tr("Error"), tr("Crashed") );
    }
    else if ( exitCode != 0 )
    {
        //QMessageBox::warning( this, tr("Error"), tr("Failed") );
    }
    else
    {
        // 正常終了時の処理
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(QFile::exists(QCoreApplication::applicationDirPath()+"/temp.ass")){
        QFile::remove(QCoreApplication::applicationDirPath()+"/temp.ass");
    }

    stopRender = true;


    QSettings settings(QCoreApplication::applicationDirPath()+"/settings.ini", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    settings.beginGroup("Kick");
    settings.setValue("path", kickPath);
    settings.setValue("disableRenderWindow", disableRenderWindow);
    settings.setValue("disableProgressiveRender", disableProgressiveRender);
    settings.endGroup();

    settings.sync();
}
