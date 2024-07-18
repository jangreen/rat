#pragma once

#include <boost/flyweight/flyweight.hpp>
#include <boost/flyweight/no_locking.hpp>
#include <boost/flyweight/no_tracking.hpp>
#include <boost/flyweight/static_holder.hpp>
#include <boost/flyweight/hashed_factory.hpp>

typedef boost::flyweight<std::string, boost::flyweights::no_locking, boost::flyweights::no_tracking,
boost::flyweights::static_holder> CanonicalString;