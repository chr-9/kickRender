#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QFile>
#include <QFileDialog>
#include <QSettings>
#include <QTextCodec>
#include <QTextStream>
#include <QProcess>
#include <QtConcurrent>
#include <chrono>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_selectTexPath_clicked();

    void on_pushButton_selectOutPathTo_clicked();

    void on_pushButton_load_clicked();

    void on_pushButton_selectKickPath_clicked();

    void on_pushButton_Render_clicked();

    void proc_finished( int exitCode, QProcess::ExitStatus exitStatus );

    void updateOutput();

    void updateError();

    void on_checkBox_stateChanged();

    void on_checkBox_2_stateChanged();

    void on_checkBox_SkipLicenseCheck_stateChanged();

    void AppendLog(QString str);

    QString sectoqstr(int sec);

    void on_pushButton_StopRender_clicked();

private:
    Ui::MainWindow *ui;
    QProcess m_proc;

protected:
    void closeEvent(QCloseEvent *);
};
#endif // MAINWINDOW_H
