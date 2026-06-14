//
// Generated file, do not edit! Created by opp_msgtool 6.4 from ospf.msg.
//

// Disable warnings about unused variables, empty switch stmts, etc:
#ifdef _MSC_VER
#  pragma warning(disable:4101)
#  pragma warning(disable:4065)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wc++98-compat"
#  pragma clang diagnostic ignored "-Wunreachable-code-break"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wshadow"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#  pragma GCC diagnostic ignored "-Wfloat-conversion"
#endif

#include <iostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include "ospf_m.h"

namespace omnetpp {

// Template pack/unpack rules. They are declared *after* a1l type-specific pack functions for multiple reasons.
// They are in the omnetpp namespace, to allow them to be found by argument-dependent lookup via the cCommBuffer argument

// Packing/unpacking an std::vector
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::vector<T,A>& v)
{
    int n = v.size();
    doParsimPacking(buffer, n);
    for (int i = 0; i < n; i++)
        doParsimPacking(buffer, v[i]);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::vector<T,A>& v)
{
    int n;
    doParsimUnpacking(buffer, n);
    v.resize(n);
    for (int i = 0; i < n; i++)
        doParsimUnpacking(buffer, v[i]);
}

// Packing/unpacking an std::list
template<typename T, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::list<T,A>& l)
{
    doParsimPacking(buffer, (int)l.size());
    for (typename std::list<T,A>::const_iterator it = l.begin(); it != l.end(); ++it)
        doParsimPacking(buffer, (T&)*it);
}

template<typename T, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::list<T,A>& l)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        l.push_back(T());
        doParsimUnpacking(buffer, l.back());
    }
}

// Packing/unpacking an std::set
template<typename T, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::set<T,Tr,A>& s)
{
    doParsimPacking(buffer, (int)s.size());
    for (typename std::set<T,Tr,A>::const_iterator it = s.begin(); it != s.end(); ++it)
        doParsimPacking(buffer, *it);
}

template<typename T, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::set<T,Tr,A>& s)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        T x;
        doParsimUnpacking(buffer, x);
        s.insert(x);
    }
}

// Packing/unpacking an std::map
template<typename K, typename V, typename Tr, typename A>
void doParsimPacking(omnetpp::cCommBuffer *buffer, const std::map<K,V,Tr,A>& m)
{
    doParsimPacking(buffer, (int)m.size());
    for (typename std::map<K,V,Tr,A>::const_iterator it = m.begin(); it != m.end(); ++it) {
        doParsimPacking(buffer, it->first);
        doParsimPacking(buffer, it->second);
    }
}

template<typename K, typename V, typename Tr, typename A>
void doParsimUnpacking(omnetpp::cCommBuffer *buffer, std::map<K,V,Tr,A>& m)
{
    int n;
    doParsimUnpacking(buffer, n);
    for (int i = 0; i < n; i++) {
        K k; V v;
        doParsimUnpacking(buffer, k);
        doParsimUnpacking(buffer, v);
        m[k] = v;
    }
}

// Default pack/unpack function for arrays
template<typename T>
void doParsimArrayPacking(omnetpp::cCommBuffer *b, const T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimPacking(b, t[i]);
}

template<typename T>
void doParsimArrayUnpacking(omnetpp::cCommBuffer *b, T *t, int n)
{
    for (int i = 0; i < n; i++)
        doParsimUnpacking(b, t[i]);
}

// Default rule to prevent compiler from choosing base class' doParsimPacking() function
template<typename T>
void doParsimPacking(omnetpp::cCommBuffer *, const T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimPacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

template<typename T>
void doParsimUnpacking(omnetpp::cCommBuffer *, T& t)
{
    throw omnetpp::cRuntimeError("Parsim error: No doParsimUnpacking() function for type %s", omnetpp::opp_typename(typeid(t)));
}

}  // namespace omnetpp


template<typename T>
std::string toStringIfPrintable(const T& t) {
    if constexpr (omnetpp::internal::is_printable<T>::value) {
        std::ostringstream os;
        os << t;
        return os.str();
    }
    return omnetpp::cClassDescriptor::UNPRINTABLE;
}

template<typename T>
bool fromStringIfExtractable(T& t, const char *s) {
    if constexpr (omnetpp::internal::is_extractable<T>::value) {
        std::istringstream is(s);
        is >> t;
        return true;
    }
    return false;
}

Register_Class(OspfMess)

OspfMess::OspfMess(const char *name, short kind) : ::omnetpp::cMessage(name, kind)
{
}

OspfMess::OspfMess(const OspfMess& other) : ::omnetpp::cMessage(other)
{
    copy(other);
}

OspfMess::~OspfMess()
{
    delete [] this->payload;
}

OspfMess& OspfMess::operator=(const OspfMess& other)
{
    if (this == &other) return *this;
    ::omnetpp::cMessage::operator=(other);
    copy(other);
    return *this;
}

void OspfMess::copy(const OspfMess& other)
{
    this->version = other.version;
    this->type = other.type;
    this->length = other.length;
    this->routerId = other.routerId;
    this->areaId = other.areaId;
    this->checksum = other.checksum;
    this->authType = other.authType;
    this->authData1 = other.authData1;
    this->authData2 = other.authData2;
    delete [] this->payload;
    this->payload = (other.payload_arraysize==0) ? nullptr : new uint8_t[other.payload_arraysize];
    payload_arraysize = other.payload_arraysize;
    for (size_t i = 0; i < payload_arraysize; i++) {
        this->payload[i] = other.payload[i];
    }
}

void OspfMess::parsimPack(omnetpp::cCommBuffer *b) const
{
    ::omnetpp::cMessage::parsimPack(b);
    doParsimPacking(b,this->version);
    doParsimPacking(b,this->type);
    doParsimPacking(b,this->length);
    doParsimPacking(b,this->routerId);
    doParsimPacking(b,this->areaId);
    doParsimPacking(b,this->checksum);
    doParsimPacking(b,this->authType);
    doParsimPacking(b,this->authData1);
    doParsimPacking(b,this->authData2);
    b->pack(payload_arraysize);
    doParsimArrayPacking(b,this->payload,payload_arraysize);
}

void OspfMess::parsimUnpack(omnetpp::cCommBuffer *b)
{
    ::omnetpp::cMessage::parsimUnpack(b);
    doParsimUnpacking(b,this->version);
    doParsimUnpacking(b,this->type);
    doParsimUnpacking(b,this->length);
    doParsimUnpacking(b,this->routerId);
    doParsimUnpacking(b,this->areaId);
    doParsimUnpacking(b,this->checksum);
    doParsimUnpacking(b,this->authType);
    doParsimUnpacking(b,this->authData1);
    doParsimUnpacking(b,this->authData2);
    delete [] this->payload;
    b->unpack(payload_arraysize);
    if (payload_arraysize == 0) {
        this->payload = nullptr;
    } else {
        this->payload = new uint8_t[payload_arraysize];
        doParsimArrayUnpacking(b,this->payload,payload_arraysize);
    }
}

uint8_t OspfMess::getVersion() const
{
    return this->version;
}

void OspfMess::setVersion(uint8_t version)
{
    this->version = version;
}

uint8_t OspfMess::getType() const
{
    return this->type;
}

void OspfMess::setType(uint8_t type)
{
    this->type = type;
}

uint16_t OspfMess::getLength() const
{
    return this->length;
}

void OspfMess::setLength(uint16_t length)
{
    this->length = length;
}

uint32_t OspfMess::getRouterId() const
{
    return this->routerId;
}

void OspfMess::setRouterId(uint32_t routerId)
{
    this->routerId = routerId;
}

uint32_t OspfMess::getAreaId() const
{
    return this->areaId;
}

void OspfMess::setAreaId(uint32_t areaId)
{
    this->areaId = areaId;
}

uint16_t OspfMess::getChecksum() const
{
    return this->checksum;
}

void OspfMess::setChecksum(uint16_t checksum)
{
    this->checksum = checksum;
}

uint16_t OspfMess::getAuthType() const
{
    return this->authType;
}

void OspfMess::setAuthType(uint16_t authType)
{
    this->authType = authType;
}

uint32_t OspfMess::getAuthData1() const
{
    return this->authData1;
}

void OspfMess::setAuthData1(uint32_t authData1)
{
    this->authData1 = authData1;
}

uint32_t OspfMess::getAuthData2() const
{
    return this->authData2;
}

void OspfMess::setAuthData2(uint32_t authData2)
{
    this->authData2 = authData2;
}

size_t OspfMess::getPayloadArraySize() const
{
    return payload_arraysize;
}

uint8_t OspfMess::getPayload(size_t k) const
{
    if (k >= payload_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)payload_arraysize, (unsigned long)k);
    return this->payload[k];
}

void OspfMess::setPayloadArraySize(size_t newSize)
{
    uint8_t *payload2 = (newSize==0) ? nullptr : new uint8_t[newSize];
    size_t minSize = payload_arraysize < newSize ? payload_arraysize : newSize;
    for (size_t i = 0; i < minSize; i++)
        payload2[i] = this->payload[i];
    for (size_t i = minSize; i < newSize; i++)
        payload2[i] = 0;
    delete [] this->payload;
    this->payload = payload2;
    payload_arraysize = newSize;
}

void OspfMess::setPayload(size_t k, uint8_t payload)
{
    if (k >= payload_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)payload_arraysize, (unsigned long)k);
    this->payload[k] = payload;
}

void OspfMess::insertPayload(size_t k, uint8_t payload)
{
    if (k > payload_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)payload_arraysize, (unsigned long)k);
    size_t newSize = payload_arraysize + 1;
    uint8_t *payload2 = new uint8_t[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        payload2[i] = this->payload[i];
    payload2[k] = payload;
    for (i = k + 1; i < newSize; i++)
        payload2[i] = this->payload[i-1];
    delete [] this->payload;
    this->payload = payload2;
    payload_arraysize = newSize;
}

void OspfMess::appendPayload(uint8_t payload)
{
    insertPayload(payload_arraysize, payload);
}

void OspfMess::erasePayload(size_t k)
{
    if (k >= payload_arraysize) throw omnetpp::cRuntimeError("Array of size %lu indexed by %lu", (unsigned long)payload_arraysize, (unsigned long)k);
    size_t newSize = payload_arraysize - 1;
    uint8_t *payload2 = (newSize == 0) ? nullptr : new uint8_t[newSize];
    size_t i;
    for (i = 0; i < k; i++)
        payload2[i] = this->payload[i];
    for (i = k; i < newSize; i++)
        payload2[i] = this->payload[i+1];
    delete [] this->payload;
    this->payload = payload2;
    payload_arraysize = newSize;
}

class OspfMessDescriptor : public omnetpp::cClassDescriptor
{
  private:
    mutable const char **propertyNames;
    enum FieldConstants {
        FIELD_version,
        FIELD_type,
        FIELD_length,
        FIELD_routerId,
        FIELD_areaId,
        FIELD_checksum,
        FIELD_authType,
        FIELD_authData1,
        FIELD_authData2,
        FIELD_payload,
    };
  public:
    OspfMessDescriptor();
    virtual ~OspfMessDescriptor();

    virtual bool doesSupport(omnetpp::cObject *obj) const override;
    virtual const char **getPropertyNames() const override;
    virtual const char *getProperty(const char *propertyName) const override;
    virtual std::string getValueAsString(omnetpp::any_ptr object) const override;
    virtual void setValueAsString(omnetpp::any_ptr object, const char *value) const override;
    virtual int getFieldCount() const override;
    virtual const char *getFieldName(int field) const override;
    virtual int findField(const char *fieldName) const override;
    virtual unsigned int getFieldTypeFlags(int field) const override;
    virtual const char *getFieldTypeString(int field) const override;
    virtual const char **getFieldPropertyNames(int field) const override;
    virtual const char *getFieldProperty(int field, const char *propertyName) const override;
    virtual int getFieldArraySize(omnetpp::any_ptr object, int field) const override;
    virtual void setFieldArraySize(omnetpp::any_ptr object, int field, int size) const override;

    virtual const char *getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const override;
    virtual std::string getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const override;
    virtual omnetpp::cValue getFieldValue(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const override;

    virtual const char *getFieldStructName(int field) const override;
    virtual omnetpp::any_ptr getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const override;
    virtual void setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const override;
};

Register_ClassDescriptor(OspfMessDescriptor)

OspfMessDescriptor::OspfMessDescriptor() : omnetpp::cClassDescriptor(omnetpp::opp_typename(typeid(OspfMess)), "omnetpp::cMessage")
{
    propertyNames = nullptr;
}

OspfMessDescriptor::~OspfMessDescriptor()
{
    delete[] propertyNames;
}

bool OspfMessDescriptor::doesSupport(omnetpp::cObject *obj) const
{
    return dynamic_cast<OspfMess *>(obj)!=nullptr;
}

const char **OspfMessDescriptor::getPropertyNames() const
{
    if (!propertyNames) {
        static const char *names[] = {  nullptr };
        omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
        const char **baseNames = base ? base->getPropertyNames() : nullptr;
        propertyNames = mergeLists(baseNames, names);
    }
    return propertyNames;
}

const char *OspfMessDescriptor::getProperty(const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? base->getProperty(propertyName) : nullptr;
}

std::string OspfMessDescriptor::getValueAsString(omnetpp::any_ptr object) const
{
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    return ((cObject*)pp)->str();
}

void OspfMessDescriptor::setValueAsString(omnetpp::any_ptr object, const char *value) const
{
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    if (!fromStringIfExtractable(*pp, value))
        cClassDescriptor::setValueAsString(object, value);
}

int OspfMessDescriptor::getFieldCount() const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    return base ? 10+base->getFieldCount() : 10;
}

unsigned int OspfMessDescriptor::getFieldTypeFlags(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeFlags(field);
        field -= base->getFieldCount();
    }
    static unsigned int fieldTypeFlags[] = {
        FD_ISEDITABLE,    // FIELD_version
        FD_ISEDITABLE,    // FIELD_type
        FD_ISEDITABLE,    // FIELD_length
        FD_ISEDITABLE,    // FIELD_routerId
        FD_ISEDITABLE,    // FIELD_areaId
        FD_ISEDITABLE,    // FIELD_checksum
        FD_ISEDITABLE,    // FIELD_authType
        FD_ISEDITABLE,    // FIELD_authData1
        FD_ISEDITABLE,    // FIELD_authData2
        FD_ISARRAY | FD_ISEDITABLE | FD_ISRESIZABLE,    // FIELD_payload
    };
    return (field >= 0 && field < 10) ? fieldTypeFlags[field] : 0;
}

const char *OspfMessDescriptor::getFieldName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldName(field);
        field -= base->getFieldCount();
    }
    static const char *fieldNames[] = {
        "version",
        "type",
        "length",
        "routerId",
        "areaId",
        "checksum",
        "authType",
        "authData1",
        "authData2",
        "payload",
    };
    return (field >= 0 && field < 10) ? fieldNames[field] : nullptr;
}

int OspfMessDescriptor::findField(const char *fieldName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    int baseIndex = base ? base->getFieldCount() : 0;
    if (strcmp(fieldName, "version") == 0) return baseIndex + 0;
    if (strcmp(fieldName, "type") == 0) return baseIndex + 1;
    if (strcmp(fieldName, "length") == 0) return baseIndex + 2;
    if (strcmp(fieldName, "routerId") == 0) return baseIndex + 3;
    if (strcmp(fieldName, "areaId") == 0) return baseIndex + 4;
    if (strcmp(fieldName, "checksum") == 0) return baseIndex + 5;
    if (strcmp(fieldName, "authType") == 0) return baseIndex + 6;
    if (strcmp(fieldName, "authData1") == 0) return baseIndex + 7;
    if (strcmp(fieldName, "authData2") == 0) return baseIndex + 8;
    if (strcmp(fieldName, "payload") == 0) return baseIndex + 9;
    return base ? base->findField(fieldName) : -1;
}

const char *OspfMessDescriptor::getFieldTypeString(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldTypeString(field);
        field -= base->getFieldCount();
    }
    static const char *fieldTypeStrings[] = {
        "uint8_t",    // FIELD_version
        "uint8_t",    // FIELD_type
        "uint16_t",    // FIELD_length
        "uint32_t",    // FIELD_routerId
        "uint32_t",    // FIELD_areaId
        "uint16_t",    // FIELD_checksum
        "uint16_t",    // FIELD_authType
        "uint32_t",    // FIELD_authData1
        "uint32_t",    // FIELD_authData2
        "uint8_t",    // FIELD_payload
    };
    return (field >= 0 && field < 10) ? fieldTypeStrings[field] : nullptr;
}

const char **OspfMessDescriptor::getFieldPropertyNames(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldPropertyNames(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

const char *OspfMessDescriptor::getFieldProperty(int field, const char *propertyName) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldProperty(field, propertyName);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    }
}

int OspfMessDescriptor::getFieldArraySize(omnetpp::any_ptr object, int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldArraySize(object, field);
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        case FIELD_payload: return pp->getPayloadArraySize();
        default: return 0;
    }
}

void OspfMessDescriptor::setFieldArraySize(omnetpp::any_ptr object, int field, int size) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldArraySize(object, field, size);
            return;
        }
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        case FIELD_payload: pp->setPayloadArraySize(size); break;
        default: throw omnetpp::cRuntimeError("Cannot set array size of field %d of class 'OspfMess'", field);
    }
}

const char *OspfMessDescriptor::getFieldDynamicTypeString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldDynamicTypeString(object,field,i);
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        default: return nullptr;
    }
}

std::string OspfMessDescriptor::getFieldValueAsString(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValueAsString(object,field,i);
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        case FIELD_version: return ulong2string(pp->getVersion());
        case FIELD_type: return ulong2string(pp->getType());
        case FIELD_length: return ulong2string(pp->getLength());
        case FIELD_routerId: return ulong2string(pp->getRouterId());
        case FIELD_areaId: return ulong2string(pp->getAreaId());
        case FIELD_checksum: return ulong2string(pp->getChecksum());
        case FIELD_authType: return ulong2string(pp->getAuthType());
        case FIELD_authData1: return ulong2string(pp->getAuthData1());
        case FIELD_authData2: return ulong2string(pp->getAuthData2());
        case FIELD_payload: return ulong2string(pp->getPayload(i));
        default: return "";
    }
}

void OspfMessDescriptor::setFieldValueAsString(omnetpp::any_ptr object, int field, int i, const char *value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValueAsString(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        case FIELD_version: pp->setVersion(string2ulong(value)); break;
        case FIELD_type: pp->setType(string2ulong(value)); break;
        case FIELD_length: pp->setLength(string2ulong(value)); break;
        case FIELD_routerId: pp->setRouterId(string2ulong(value)); break;
        case FIELD_areaId: pp->setAreaId(string2ulong(value)); break;
        case FIELD_checksum: pp->setChecksum(string2ulong(value)); break;
        case FIELD_authType: pp->setAuthType(string2ulong(value)); break;
        case FIELD_authData1: pp->setAuthData1(string2ulong(value)); break;
        case FIELD_authData2: pp->setAuthData2(string2ulong(value)); break;
        case FIELD_payload: pp->setPayload(i,string2ulong(value)); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OspfMess'", field);
    }
}

omnetpp::cValue OspfMessDescriptor::getFieldValue(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldValue(object,field,i);
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        case FIELD_version: return (omnetpp::intval_t)(pp->getVersion());
        case FIELD_type: return (omnetpp::intval_t)(pp->getType());
        case FIELD_length: return (omnetpp::intval_t)(pp->getLength());
        case FIELD_routerId: return (omnetpp::intval_t)(pp->getRouterId());
        case FIELD_areaId: return (omnetpp::intval_t)(pp->getAreaId());
        case FIELD_checksum: return (omnetpp::intval_t)(pp->getChecksum());
        case FIELD_authType: return (omnetpp::intval_t)(pp->getAuthType());
        case FIELD_authData1: return (omnetpp::intval_t)(pp->getAuthData1());
        case FIELD_authData2: return (omnetpp::intval_t)(pp->getAuthData2());
        case FIELD_payload: return (omnetpp::intval_t)(pp->getPayload(i));
        default: throw omnetpp::cRuntimeError("Cannot return field %d of class 'OspfMess' as cValue -- field index out of range?", field);
    }
}

void OspfMessDescriptor::setFieldValue(omnetpp::any_ptr object, int field, int i, const omnetpp::cValue& value) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldValue(object, field, i, value);
            return;
        }
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        case FIELD_version: pp->setVersion(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        case FIELD_type: pp->setType(omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        case FIELD_length: pp->setLength(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_routerId: pp->setRouterId(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_areaId: pp->setAreaId(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_checksum: pp->setChecksum(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_authType: pp->setAuthType(omnetpp::checked_int_cast<uint16_t>(value.intValue())); break;
        case FIELD_authData1: pp->setAuthData1(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_authData2: pp->setAuthData2(omnetpp::checked_int_cast<uint32_t>(value.intValue())); break;
        case FIELD_payload: pp->setPayload(i,omnetpp::checked_int_cast<uint8_t>(value.intValue())); break;
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OspfMess'", field);
    }
}

const char *OspfMessDescriptor::getFieldStructName(int field) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructName(field);
        field -= base->getFieldCount();
    }
    switch (field) {
        default: return nullptr;
    };
}

omnetpp::any_ptr OspfMessDescriptor::getFieldStructValuePointer(omnetpp::any_ptr object, int field, int i) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount())
            return base->getFieldStructValuePointer(object, field, i);
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        default: return omnetpp::any_ptr(nullptr);
    }
}

void OspfMessDescriptor::setFieldStructValuePointer(omnetpp::any_ptr object, int field, int i, omnetpp::any_ptr ptr) const
{
    omnetpp::cClassDescriptor *base = getBaseClassDescriptor();
    if (base) {
        if (field < base->getFieldCount()){
            base->setFieldStructValuePointer(object, field, i, ptr);
            return;
        }
        field -= base->getFieldCount();
    }
    OspfMess *pp = omnetpp::fromAnyPtr<OspfMess>(object); (void)pp;
    switch (field) {
        default: throw omnetpp::cRuntimeError("Cannot set field %d of class 'OspfMess'", field);
    }
}

namespace omnetpp {

}  // namespace omnetpp

