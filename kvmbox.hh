#include <QObject>

class Vcpu : public QObject {
	Q_OBJECT
	struct kvm *kvm;
	long unsigned int threadId; // not portable

public:
	Vcpu(struct kvm *kvm);
	void interrupt();

public slots:
	void init();
	void run();

signals:
	void initialised();
	void failure();
	void exit();
};
