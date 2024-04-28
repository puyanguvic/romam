/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "romam-application-helper.h"

#include "ns3/inet-socket-address.h"
#include "ns3/names.h"
#include "ns3/romam-module.h"
#include "ns3/string.h"

namespace ns3
{

RomamApplicationHelper::RomamApplicationHelper(std::string protocol, Address address)
{
    m_factory.SetTypeId("ns3::RomamSink");
    m_factory.Set("Protocol", StringValue(protocol));
    m_factory.Set("Local", AddressValue(address));
}

RomamApplicationHelper::~RomamApplicationHelper()
{
}

void
RomamApplicationHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
RomamApplicationHelper::Install(Ptr<Node> node) const
{
    return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer
RomamApplicationHelper::Install(std::string nodeName) const
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer
RomamApplicationHelper::Install(NodeContainer c) const
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        apps.Add(InstallPriv(*i));
    }

    return apps;
}

Ptr<Application>
RomamApplicationHelper::InstallPriv(Ptr<Node> node) const
{
    Ptr<Application> app = m_factory.Create<Application>();
    node->AddApplication(app);

    return app;
}

} // namespace ns3