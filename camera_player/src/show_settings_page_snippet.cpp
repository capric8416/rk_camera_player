
void MainWindow::show_settings_page()
{
    ui->stackedWidget->setCurrentWidget(ui->settingsPage);
    LOG_INFO("Switched to settings page");
}
