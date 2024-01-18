#include "ns3/core-module.h"
#include "ns3/open-routing-module.h"

/**
 * \file
 *
 * Explain here what the example does.
 */

using namespace ns3;

int
main(int argc, char* argv[])
{
    bool verbose = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("verbose", "Tell application to log if true", verbose);

    cmd.Parse(argc, argv);

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

    

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
