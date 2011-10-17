#include <QObject>

class Vcpu : public QObject {
	Q_OBJECT
	struct kvm *kvm;

public:
	// Not portable
	long unsigned int threadId;
	Vcpu(struct kvm *kvm);

public slots:
	void init();
	void run();

signals:
	void initialised();
	void failure();
	void exit();
};
