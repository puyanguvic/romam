#include "ns3/core-module.h"
#include "ns3/open-routing-module.h"
#include "ns3/ipv4-address.h"

/**
 * \file
 *
 * Explain here what the example does.
 */

using namespace ns3;

void TestOspfHeader ()
{
    // Create an OSPFHeader instance and set its fields
    open_routing::OSPFHeader ospfHeader;
    ospfHeader.SetType(1); // Example type
    ospfHeader.SetLength(48); // Example length
    ospfHeader.SetRouterID(12345); // Example Router ID
    ospfHeader.SetAreaID(67890); // Example Area ID
    ospfHeader.SetChecksum(54321); // Example checksum
    ospfHeader.SetAuType(2); // Example Authentication type
    ospfHeader.SetAuthentication(1234567890); // Example Authentication data

    // Serialize the OSPF header to a buffer
    Buffer buffer;
    buffer.AddAtEnd(ospfHeader.GetSerializedSize());
    Buffer::Iterator it = buffer.Begin();
    ospfHeader.Serialize(it);

    // Now, deserialize the OSPF header from the buffer
    open_routing::OSPFHeader deserializedHeader;
    it = buffer.Begin();
    deserializedHeader.Deserialize(it);

    // Print the deserialized header (for demonstration)
    std::cout << "Deserialized OSPF Header:" << std::endl;
    deserializedHeader.Print(std::cout);

}

void TestHelloHeader ()
{
    open_routing::HelloHeader hello;
    hello.SetNetworkMask(Ipv4Address("255.255.255.0")); // Example network mask
    hello.SetHelloInterval(10); // Example hello interval in seconds
    hello.SetOptions(0); // Example options value
    uint8_t pri = 1;
    hello.SetRouterPriority(pri); // Example router priority
    hello.SetRouterDeadInterval(40); // Example router dead interval in seconds
    hello.SetDesignatedRouter(Ipv4Address("192.168.1.1")); // Example DR address
    hello.SetBackupDesignatedRouter(Ipv4Address("192.168.1.2")); // Example BDR address

    // Add two random neighbors
    hello.AddNeighbor(Ipv4Address("192.168.1.3")); // Neighbor 1
    hello.AddNeighbor(Ipv4Address("192.168.1.4")); // Neighbor 2

    // Serialize the header to a buffer
    Buffer buffer;
    buffer.AddAtEnd(hello.GetSerializedSize());
    Buffer::Iterator it = buffer.Begin();
    hello.Serialize(it);

    // Now, deserialize the header from the buffer
    open_routing::HelloHeader deserializedHeader;
    it = buffer.Begin();
    deserializedHeader.Deserialize(it);

    // Print the deserialized header (for demonstration)
    std::cout << "Deserialized Hello Header:" << std::endl;
    deserializedHeader.Print(std::cout);
}

int
main(int argc, char* argv[])
{
    bool verbose = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell application to log if true", verbose);

    cmd.Parse(argc, argv);

    // TestOspfHeader ();
    TestHelloHeader ();

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
