#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    return doctest::Context(argc, argv).run();
}
