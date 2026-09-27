#include <QApplication>
#include <QSettings>

#include "app_model.hpp"
#include "main_window.hpp"
#include "session_folder.hpp"
#include "theme.hpp"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("WorkoutLog");
    QApplication::setApplicationName("WorkoutLog");
    theme::apply(app);

    auto remembered = QSettings().value("workout_log.folder").toString();
    AppModel model;
    MainWindow window(model);
    model.start(folder_at(default_session_directory(
        remembered.isEmpty() ? std::nullopt : std::optional<std::string>(ss(remembered)))));
    window.show();
    return QApplication::exec();
}
