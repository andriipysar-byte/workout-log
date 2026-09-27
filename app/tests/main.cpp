#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    return doctest::Context(argc, argv).run();
}
