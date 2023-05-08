import sqlite3
import os

class DB:
    def __init__(self, config):
        self.config = config
        self.conn = sqlite3.connect(config["db_path"])
        self.cursor = self.conn.cursor()
        self.create_tables()

    def __del__(self):
        if self.config['delete_db_on_exit']:
            os.remove(self.config['db_path'])

    def close(self):
        self.conn.close()

    def insert_subflow(self, iteration_id, subflow_id, local_addr, remote_addr):
        self.cursor.execute("""
            INSERT INTO subflows (iteration_id, subflow_id, local_addr, remote_addr)
            VALUES (:iteration_id, :subflow_id, :local_addr, :remote_addr)
        """, { "iteration_id": iteration_id, "subflow_id": subflow_id, "local_addr": local_addr, "remote_addr": remote_addr })
        self.conn.commit()

    def read_subflow(self, iteration_id, subflow_id):
        self.cursor.execute("""
            SELECT *
            FROM subflows
            WHERE iteration_id = :iteration_id AND subflow_id = :subflow_id
        """, { "iteration_id": iteration_id, "subflow_id": subflow_id })
        return self.cursor.fetchone()

    def read_all_subflows(self, iteration_id):
        self.cursor.execute("""
            SELECT *
            FROM subflows
            WHERE iteration_id = :iteration_id
        """, { "iteration_id": iteration_id })
        return self.cursor.fetchall()


    def insert_subflow_tcp_conn_info(self, iteration_id, subflow_id, tcpi):
        # Not all fields may be present, so we need to check for each one
        # and insert NULL if it's not present
        present_fields = [ key for key, value in tcpi.items() if value is not None ]
        fields = ", ".join(present_fields)
        values = ", ".join([ f":{field}" for field in present_fields ])
        self.cursor.execute(f"""
            INSERT INTO tcp_conn_info (iteration_id, subflow_id, {fields})
            VALUES (:iteration_id, :subflow_id, {values})
        """, { **tcpi, "iteration_id": iteration_id, "subflow_id": subflow_id })
        self.conn.commit()

    def read_subflow_tcp_conn_info(self, iteration_id, subflow_id, fields):
        fields = ", ".join(fields)
        self.cursor.execute(f"""
            SELECT {fields}
            FROM tcp_conn_info
            WHERE iteration_id = :iteration_id AND subflow_id = :subflow_id
        """, { "iteration_id": iteration_id, "subflow_id": subflow_id })
        return self.cursor.fetchall()


    def create_tables(self):
        #  Creat table to track subflows
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS subflows (
                iteration_id STRING,
                subflow_id INTEGER,
                local_addr STRING,
                remote_addr STRING
            );
        """)
        self.cursor.execute("""
            CREATE INDEX IF NOT EXISTS subflows_index ON subflows (iteration_id, subflow_id);
        """)
        self.conn.commit()

        # Need table for tcp connection info
        # Store a unique iteration ID to know what iteration each row belongs to
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS tcp_conn_info (
                iteration_id STRING,
                subflow_id INTEGER,
                tcpi_retransmits INTEGER,
                tcpi_probes INTEGER,
                tcpi_backoff INTEGER,
                tcpi_options INTEGER,
                tcpi_snd_wscale INTEGER,
                tcpi_rcv_wscale INTEGER,
                tcpi_delivery_rate_app_limited INTEGER,
                tcpi_fastopen_client_fail INTEGER,
                tcpi_rto INTEGER,
                tcpi_ato INTEGER,
                tcpi_snd_mss INTEGER,
                tcpi_rcv_mss INTEGER,
                tcpi_unacked INTEGER,
                tcpi_sacked INTEGER,
                tcpi_lost INTEGER,
                tcpi_retrans INTEGER,
                tcpi_fackets INTEGER,
                tcpi_last_data_sent INTEGER,
                tcpi_last_ack_sent INTEGER,
                tcpi_last_data_recv INTEGER,
                tcpi_last_ack_recv INTEGER,
                tcpi_pmtu INTEGER,
                tcpi_rcv_ssthresh INTEGER,
                tcpi_rtt INTEGER,
                tcpi_rttvar INTEGER,
                tcpi_snd_ssthresh INTEGER,
                tcpi_snd_cwnd INTEGER,
                tcpi_advmss INTEGER,
                tcpi_reordering INTEGER,
                tcpi_rcv_rtt INTEGER,
                tcpi_rcv_space INTEGER,
                tcpi_total_retrans INTEGER,
                tcpi_pacing_rate INTEGER,
                tcpi_max_pacing_rate INTEGER,
                tcpi_bytes_acked INTEGER,
                tcpi_bytes_received INTEGER,
                tcpi_segs_out INTEGER,
                tcpi_segs_in INTEGER,
                tcpi_notsent_bytes INTEGER,
                tcpi_min_rtt INTEGER,
                tcpi_data_segs_in INTEGER,
                tcpi_data_segs_out INTEGER,
                tcpi_delivery_rate INTEGER,
                tcpi_busy_time INTEGER,
                tcpi_rwnd_limited INTEGER,
                tcpi_sndbuf_limited INTEGER,
                tcpi_delivered INTEGER,
                tcpi_delivered_ce INTEGER,
                tcpi_bytes_sent INTEGER,
                tcpi_bytes_retrans INTEGER,
                tcpi_dsack_dups INTEGER,
                tcpi_reord_seen INTEGER,
                tcpi_rcv_ooopack INTEGER
            );
        """)
        self.cursor.execute("""
            CREATE INDEX IF NOT EXISTS tcp_conn_info_index ON tcp_conn_info (iteration_id, subflow_id);
        """)
        
        self.conn.commit()


                
