#include <QtGui>
#include <cstdio>
#include <cstdarg>
#include "kvmbox.h"
#include "kvmbox.hh"

QTextEdit *textEdit;

extern "C" void debugf(const char *format, ...) {
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

	struct kvm *kvm = vm_init(argv, args);
	Vcpu *vcpu = new Vcpu(kvm);

	QThread *vcpuThread = new QThread();
	vcpu->connect(vcpuThread, SIGNAL(started()), SLOT(init()));
	vcpu->connect(vcpu, SIGNAL(initialised()), SLOT(run()));
	vcpu->connect(vcpu, SIGNAL(exit()), SLOT(run()));
	vcpuThread->connect(vcpu, SIGNAL(failure()), SLOT(quit()));

	QObject::connect(quitButton, SIGNAL(clicked()), qApp, SLOT(quit()));

	vcpu->moveToThread(vcpuThread);

	QVBoxLayout *layout = new QVBoxLayout();
	layout->addWidget(textEdit);
	layout->addWidget(quitButton);

	QWidget window;
	window.setLayout(layout);

	window.show();
	vcpuThread->start();

	int ret = app.exec();
	vcpu->interrupt();
	vcpuThread->quit();
	vcpuThread->wait();
	return ret;
}
