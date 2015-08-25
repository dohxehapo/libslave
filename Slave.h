/* Copyright 2011 ZAO "Begun".
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 *
 * Guaranteed to work with MySQL5.1.23
 *
 */

#ifndef __SLAVE_SLAVE_H_
#define __SLAVE_SLAVE_H_


#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <string.h>

#include <mysql.h>

#include "slave_log_event.h"
#include "SlaveStats.h"


namespace slave
{


class Slave
{
public:

    typedef std::vector<std::pair<std::string, std::string> > table_order_t;
    typedef std::map<std::pair<std::string, std::string>, callback> callbacks_t;
    typedef std::map<std::pair<std::string, std::string>, filter> filters_t;


private:
    static inline bool falseFunction() { return false; };

    MYSQL mysql;

    int m_server_id;

    MasterInfo m_master_info;
    EmptyExtState empty_ext_state;
    ExtStateIface &ext_state;
    EventStatIface* event_stat;

    table_order_t m_table_order;
    callbacks_t m_callbacks;
    filters_t m_filters;

    typedef boost::function<void (unsigned int)> xid_callback_t;
    xid_callback_t m_xid_callback;

    RelayLogInfo m_rli;

    std::string m_reportHost;
    std::string m_reportUser;
    std::string m_reportPassword;
    unsigned int m_reportPort;

    void createDatabaseStructure_(table_order_t& tabs, RelayLogInfo& rli) const;

public:

    Slave();
    Slave(ExtStateIface &state);
    Slave(const MasterInfo& _master_info);
    Slave(const MasterInfo& _master_info, ExtStateIface &state);

    void linkEventStat(EventStatIface* _event_stat)
    {
        event_stat = _event_stat;
    }

    // Makes sense only when get_remote_binlog is not started
    void setMasterInfo(const MasterInfo& aMasterInfo)
    {
        m_master_info = aMasterInfo;
        ext_state.setMasterLogNamePos(aMasterInfo.master_log_name, aMasterInfo.master_log_pos);
    }
    const MasterInfo& masterInfo() const { return m_master_info; }

    typedef std::pair<std::string, unsigned int> binlog_pos_t;
    // Reads current binlog position from database
    binlog_pos_t getLastBinlog() const;

    void setCallback(const std::string& _db_name, const std::string& _tbl_name, callback _callback,
                     EventKind filter = eAll)
    {
        const std::pair<std::string, std::string> key = std::make_pair(_db_name, _tbl_name);
        m_table_order.push_back(key);
        m_callbacks[key] = _callback;
        m_filters[key] = filter;

        ext_state.initTableCount(_db_name + "." + _tbl_name);
    }

    void setXidCallback(xid_callback_t _callback)
    {
        m_xid_callback = _callback;
    }

    void get_remote_binlog( const boost::function< bool() >& _interruptFlag = &Slave::falseFunction );

    void createDatabaseStructure() {

        m_rli.clear();

        createDatabaseStructure_(m_table_order, m_rli);

        for (RelayLogInfo::name_to_table_t::iterator i = m_rli.m_table_map.begin(); i != m_rli.m_table_map.end(); ++i) {
            i->second->m_callback = m_callbacks[i->first];
            i->second->m_filter = m_filters[i->first];
        }
    }

    RelayLogInfo getRli() const {
        return m_rli;
    }

    table_order_t getTableOrder() const {
        return m_table_order;
    }

    void init();

    int serverId() const { return m_server_id; }

    // Closes connection, opened in get_remotee_binlog. Should be called if your have get_remote_binlog
    // blocked on reading data from mysql server in the separate thread and you want to stop this thread.
    // You should take care that interruptFlag will return 'true' after connection is closed.
    void close_connection();

    void setReportHost(const std::string& str) { m_reportHost = str; }
    void setReportUser(const std::string& str) { m_reportUser = str; }
    void setReportPassword(const std::string& str) { m_reportPassword = str; }
    void setReportPort(unsigned int port) { m_reportPort = port; }

protected:


    void check_master_version();

    void check_master_binlog_format();

    int process_event(const slave::Basic_event_info& bei, RelayLogInfo &rli, unsigned long long pos);

    void request_dump(const std::string& logname, unsigned long start_position, MYSQL* mysql);

    ulong read_event(MYSQL* mysql);

    std::map<std::string,std::string> getRowType(const std::string& db_name,
                                                 const std::set<std::string>& tbl_names) const;

    void createTable(RelayLogInfo& rli,
                     const std::string& db_name, const std::string& tbl_name,
                     const collate_map_t& collate_map, nanomysql::Connection& conn) const;

    void register_slave_on_master(MYSQL* mysql);
    void deregister_slave_on_master(MYSQL* mysql);

    void generateSlaveId();

};

}// slave

#endif
