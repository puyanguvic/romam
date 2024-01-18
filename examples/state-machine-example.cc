#include "ns3/core-module.h"
#include "ns3/open-routing-helper.h"
#include "ns3/open-routing-module.h"

using namespace ns3;

class  ConcreteStateA : public open_routing::State {
    public:
    void Handle () override;

};

class ConcreteStateB : public open_routing::State {
    void Handle () override {
        std::cout << "ConcreteStateB handles request.\n";
    }

};

void ConcreteStateA::Handle ()
{
    std::cout << "ConcreteStateA handle requests.\n";
    this->m_context->TransitionTo (new ConcreteStateB);
}

void ClientCode ()
{
    open_routing::Context *context = new open_routing::Context (new ConcreteStateA);
    context->Request ();
    delete context;
}

int
main(int argc, char* argv[])
{
    bool verbose = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell application to log if true", verbose);

    cmd.Parse(argc, argv);

    ClientCode ();

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
