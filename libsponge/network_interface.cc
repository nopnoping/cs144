#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // 1. 看是否有ip-ethernet
    // 惰性删除
    if (_ip_map_ethernet_address.count(next_hop_ip) &&
        _time_ms - _ip_map_ethernet_address[next_hop_ip].second > 30 * 1000) {
        _ip_map_ethernet_address.erase(next_hop_ip);
    }
    if (_ip_map_ethernet_address.count(next_hop_ip)) {
        send_ipv4_ethernet_frame(dgram, _ip_map_ethernet_address[next_hop_ip].first);
    } else {
        // ARP
        // 没有发出过ARP，或则距离上一次发送ARP的时间大于5s，才会发出ARP包
        if (_arp_send_time.count(next_hop_ip) == 0 || _time_ms-_arp_send_time[next_hop_ip] > 5* 1000) {
            EthernetAddress a{};
            send_arp_ethernet_frame(ARPMessage::OPCODE_REQUEST, a, next_hop_ip, ETHERNET_BROADCAST);
            _arp_send_time[next_hop_ip] = _time_ms;
        }
        _datagram_wait_queue[next_hop_ip].push(dgram);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram{};
        if (frame.header().dst == _ethernet_address &&
            dgram.parse(frame.payload()) == ParseResult::NoError) {
            return dgram;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arpMessage{};
        if (arpMessage.parse(frame.payload()) == ParseResult::NoError) {
            // update ip map ethernet
            _ip_map_ethernet_address[arpMessage.sender_ip_address].first = arpMessage.sender_ethernet_address;
            _ip_map_ethernet_address[arpMessage.sender_ip_address].second = _time_ms;

            if (arpMessage.opcode == ARPMessage::OPCODE_REQUEST &&
                arpMessage.target_ip_address == _ip_address.ipv4_numeric()) {
                send_arp_ethernet_frame(ARPMessage::OPCODE_REPLY, arpMessage.sender_ethernet_address,
                                        arpMessage.sender_ip_address, arpMessage.sender_ethernet_address);
            } else if (arpMessage.opcode == ARPMessage::OPCODE_REPLY &&
                       arpMessage.target_ip_address == _ip_address.ipv4_numeric()) {
                _arp_send_time.erase(arpMessage.sender_ip_address);
                while (!_datagram_wait_queue[arpMessage.sender_ip_address].empty()) {
                    InternetDatagram& dgram = _datagram_wait_queue[arpMessage.sender_ip_address].front();
                    send_ipv4_ethernet_frame(dgram, arpMessage.sender_ethernet_address);
                    _datagram_wait_queue[arpMessage.sender_ip_address].pop();
                }
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time_ms += ms_since_last_tick;
}


void NetworkInterface::send_ipv4_ethernet_frame(const InternetDatagram& dgram, EthernetAddress dst) {
    EthernetFrame ethernetFrame{};
    ethernetFrame.payload() = dgram.serialize();
    ethernetFrame.header().src = _ethernet_address;
    ethernetFrame.header().dst = dst;
    ethernetFrame.header().type = EthernetHeader::TYPE_IPv4;
    _frames_out.push(ethernetFrame);
}

void NetworkInterface::send_arp_ethernet_frame(uint16_t opcode, EthernetAddress target_ethernet_address,
                             uint32_t target_ip_address, EthernetAddress dst) {
    EthernetFrame ethernetFrame{};
    ARPMessage arpMessage{};
    arpMessage.opcode = opcode;
    arpMessage.sender_ethernet_address = _ethernet_address;
    arpMessage.sender_ip_address = _ip_address.ipv4_numeric();
    arpMessage.target_ethernet_address = target_ethernet_address;
    arpMessage.target_ip_address = target_ip_address;

    ethernetFrame.payload() = arpMessage.serialize();
    ethernetFrame.header().src = _ethernet_address;
    ethernetFrame.header().dst = dst;
    ethernetFrame.header().type = EthernetHeader::TYPE_ARP;
    _frames_out.push(ethernetFrame);
}
