// Copyright (c) 2011 Scott Mansell <phiren@gmail.com>
// Licensed under the MIT license
// Refer to the included LICENCE file.

#include <QtGui>
#include <cstdio>
#include <cstdarg>

QTextEdit *textEdit;

extern "C" void debugf(char *format, ...) {
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

int main(int argv, char **args) {
	QApplication app(argv, args);

	textEdit = new QTextEdit;
	textEdit->setReadOnly(TRUE);
	QPushButton *quitButton = new QPushButton("Quit");

	QObject::connect(quitButton, SIGNAL(clicked()), qApp, SLOT(quit()));

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textEdit);
	layout->addWidget(quitButton);

	QWidget window;
	window.setLayout(layout);

	window.show();

	return app.exec();
}
