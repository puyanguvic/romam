/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "romam-tcp-application-helper.h"

#include "ns3/inet-socket-address.h"
#include "ns3/names.h"
#include "ns3/packet-socket-address.h"
#include "ns3/string.h"

namespace ns3
{

RomamTcpAppHelper::RomamTcpAppHelper(std::string protocol, Address address)
{
    m_factory.SetTypeId("ns3::DGRTcpApplication");
    m_factory.Set("Protocol", StringValue(protocol));
    m_factory.Set("Remote", AddressValue(address));
}

void
RomamTcpAppHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
RomamTcpAppHelper::Install(Ptr<Node> node) const
{
    return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer
RomamTcpAppHelper::Install(std::string nodeName) const
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer
RomamTcpAppHelper::Install(NodeContainer c) const
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        apps.Add(InstallPriv(*i));
    }

    return apps;
}

Ptr<Application>
RomamTcpAppHelper::InstallPriv(Ptr<Node> node) const
{
    Ptr<Application> app = m_factory.Create<Application>();
    node->AddApplication(app);

    return app;
}

} // namespace ns3