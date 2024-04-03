/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2024
 */

#include "database.h"
#include "ns3/assert.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Database");

NS_OBJECT_ENSURE_REGISTERED(Database);

TypeId
Database::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Database").SetParent<Object>().SetGroupName("romam");
    return tid;
}

} // namespace ns3
