
#include <stdio.h>
#include <dpdk/dpdk.h>

struct ether_addr dst[2];
struct ether_addr src[2];

constexpr size_t n_queues = 4;
int l2fwd(void*)
{
  const size_t n_ports = rte_eth_dev_count();
  while (true) {
    for (size_t pid=0; pid<n_ports; pid++) {
			for (size_t qid=0; qid<n_queues; qid++) {
				constexpr size_t BURSTSZ = 32;
				rte_mbuf* mbufs[BURSTSZ];

				size_t nb_recv = rte_eth_rx_burst(pid, qid, mbufs, BURSTSZ);
				if (nb_recv == 0) continue;

        for (size_t i=0; i<nb_recv; i++) {
          uint8_t* p = rte_pktmbuf_mtod(mbufs[i], uint8_t*);
          memcpy(p+0, &src[pid], 6);
          memcpy(p+6, &dst[pid], 6);
        }
        rte_eth_tx_burst(pid^1, qid, mbufs, nb_recv);
			}
    }
  }
}

std::string to_str(const ether_addr* addr)
{
  std::string s;
  for (size_t i=0; i<6; i++) {
    s += dpdk::format("%02x%s", addr->addr_bytes[i], i<5?":":"");
  }
  return s;
}

int main(int argc, char** argv)
{

  dpdk::dpdk_boot(argc, argv);
  struct rte_eth_conf port_conf;
  dpdk::init_portconf(&port_conf);
  struct rte_mempool* mp = dpdk::mp_alloc("RXMBUFMP", 0, 8192);

  size_t n_ports = rte_eth_dev_count();
  if (n_ports == 0) throw dpdk::exception("no ethdev");
  printf("%zd ports found \n", n_ports);
  for (size_t i=0; i<n_ports; i++) {
    dpdk::port_configure(i, n_queues, n_queues, &port_conf, mp);
  }

  dst[0].addr_bytes[0] = 0x33;
  dst[0].addr_bytes[1] = 0x33;
  dst[0].addr_bytes[2] = 0x33;
  dst[0].addr_bytes[3] = 0x33;
  dst[0].addr_bytes[4] = 0x33;
  dst[0].addr_bytes[5] = 0x33;

  dst[1].addr_bytes[0] = 0xa0;
  dst[1].addr_bytes[1] = 0x36;
  dst[1].addr_bytes[2] = 0x9f;
  dst[1].addr_bytes[3] = 0x39;
  dst[1].addr_bytes[4] = 0x10;
  dst[1].addr_bytes[5] = 0x4c;

  rte_eth_macaddr_get(0, &src[0]);
  rte_eth_macaddr_get(1, &src[1]);

  printf("flow[0->1]: %s -> %s \n", to_str(&src[0]).c_str(), to_str(&dst[0]).c_str());
  printf("flow[1->0]: %s -> %s \n", to_str(&src[1]).c_str(), to_str(&dst[1]).c_str());

  rte_eal_remote_launch(l2fwd, nullptr, 1);
  rte_eal_mp_wait_lcore();
}

