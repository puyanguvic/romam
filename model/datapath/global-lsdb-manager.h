/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef GLOBAL_LSDB_MANAGER_H
#define GLOBAL_LSDB_MANAGER_H

#include <stdint.h>
#include <list>
#include <queue>
#include <map>
#include <vector>
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "lsdb.h"

namespace ns3 {

class GlobalLSDBManager
{
  public:
    GlobalLSDBManager ();
    virtual ~GlobalLSDBManager ();

    // /**
    //  * @brief Delete all static routes on all nodes that have a
    //  * DGRRouterInterface
    //  *
    //  * \todo  separate manually assigned static routes from static routes that
    //  * the global routing code injects, and only delete the latter
    //  */
    // virtual void DeleteRoutes ();

    /**
     * @brief Build the Link State Database (LSDB) by gathering Link State Advertisements
     * from each node exporting a DGRRouter interface.
     */
    virtual void BuildLinkStateDatabase ();

    /**
     * @brief Delete the Link State Database (LSDB), create a new one.
     */
    void DeleteLinkStateDatabase ();

    /**
     * @brief Get LSDB
     * @return LSDB
     */
    LSDB* GetLSDB (void) const;
  private:
    Vertex* m_spfroot; //!< the root node
    LSDB* m_lsdb; //!< the Link State DataBase (LSDB) of the Global Route Manager
};

} // namespace ns3

#endif /* GLOBAL_LSDB_MANAGER_H */