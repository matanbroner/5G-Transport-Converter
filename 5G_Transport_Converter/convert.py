from scapy.all import *

CONVERT_HEADER_LENGTH = 4

CONVERT_TLVS = {
    0x1: 'info',
    0xa: 'connect',
    0x14: 'extended',
    0x15: 'supported',
    0x16: 'cookie',
    0x1e: 'error',
}


class _ConvertTLV_HDR(Packet):
    fields_desc = [ByteEnumField(
        "type", 0, CONVERT_TLVS), ByteField("length", 1), ]


class ConvertTLV(Packet):
    name = "Convert TLV"
    fields_desc = [_ConvertTLV_HDR, ShortField("padding", 0)]

    def extract_padding(self, s):
        return '', s


class ConvertTLV_Info(ConvertTLV):
    type = 0x1


class ConvertTLV_Error(ConvertTLV):
    type = 0x1e
    fields_desc = [_ConvertTLV_HDR, ByteField(
        "error_code", 0), ByteField("value", 0), ]


class ConvertTLV_Connect(ConvertTLV):
    type = 0xa
    length = 5
    fields_desc = [_ConvertTLV_HDR, ShortField(
        "remote_port", None), IP6Field("remote_addr", None), ]


class Convert(Packet):
    name = "Convert"
    fields_desc = [ByteField("version", 1),
                   FieldLenField("total_length", None, length_of="tlvs",
                                 fmt="B", adjust=lambda _, l: int(1 + l / 4)),
                   ShortField("magic_no", 8803),
                   PacketListField("tlvs", None, ConvertTLV)]

# Utility functions

def read_convert_header(sock):
    data = sock.recv(CONVERT_HEADER_LENGTH)
    if len(data) != CONVERT_HEADER_LENGTH:
        print("Not enough data to read Convert header")
        return None
    header = Convert(data)
    # Read the TLVs
    data = sock.recv(header.total_length * 4)
    # Setup the TLVs
    # Each TLV is aligned to 4 bytes and has an attribute "length" which is the number of 4 byte words
    # the TLV takes up including the header
    # Read each TLV, append it to the list, and move the offset of raw data by the length
    # of the previous TLV
    tlvs = []
    offset = 0
    while offset < len(data):
        tlv = ConvertTLV(data[offset:])
        # make appropriate TLV class subclass
        if tlv.type == 0x1:
            tlv = ConvertTLV_Info(bytes(tlv))
        elif tlv.type == 0x1e:
            tlv = ConvertTLV_Error(bytes(tlv))
        elif tlv.type == 0xa:
            tlv = ConvertTLV_Connect(bytes(tlv))
        else:
            print("Unknown TLV type: {}".format(tlv.type))
        tlvs.append(tlv)
        offset += tlv.length * 4
    header.tlvs = tlvs
    return header


def ipv6_to_ipv4(ipv6):
    return str(ipv6).split(":")[-1]