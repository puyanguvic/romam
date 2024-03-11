/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef DATABASE_H
#define DATABASE_H

#include <iostream>
#include <string>

#include "ns3/object.h"

namespace ns3
{
namespace open_routing
{

/**
 * \ingroup openRouting
 * \brief Abstract base class for databases.
 *
 * ...
 *
 */
class Database : public Object
{
    public:
        /**
         * \brief Get the type ID.
         * \return the object TypeId
         */
        static TypeId GetTypeId();

        /**
         * \brief Print the Routing Table entries
         * 
         * \param stream The ostream the Routing table is printed to
         * \param unit The time unit to be used in the report
         */
        virtual void Print (std::ostream &os) const = 0;
};

} // namespace open-routing
} // namespace ns3

#endif /* DATABASE_H*/