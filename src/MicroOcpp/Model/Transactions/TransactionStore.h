// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2024
// MIT License

#ifndef MO_TRANSACTIONSTORE_H
#define MO_TRANSACTIONSTORE_H

#include <vector>
#include <deque>

#include <MicroOcpp/Model/Transactions/Transaction.h>
#include <MicroOcpp/Core/FilesystemAdapter.h>

namespace MicroOcpp {

class TransactionStore;

class ConnectorTransactionStore {
private:
    TransactionStore& context;
    const unsigned int connectorId;
    const ProtocolVersion& version;

    std::shared_ptr<FilesystemAdapter> filesystem;
    
    std::deque<std::weak_ptr<ITransaction>> transactions;

public:
    ConnectorTransactionStore(TransactionStore& context, unsigned int connectorId, std::shared_ptr<FilesystemAdapter> filesystem, const ProtocolVersion& version=VER_1_6_J);
    ConnectorTransactionStore(const ConnectorTransactionStore&) = delete;
    ConnectorTransactionStore(ConnectorTransactionStore&&) = delete;
    ConnectorTransactionStore& operator=(const ConnectorTransactionStore&) = delete;

    ~ConnectorTransactionStore();

    bool commit(ITransaction *transaction);

    std::shared_ptr<ITransaction> getTransaction(unsigned int txNr);
    std::shared_ptr<ITransaction> createTransaction(unsigned int txNr, bool silent = false);

    bool remove(unsigned int txNr);
};

class TransactionStore {
private:
    std::vector<std::unique_ptr<ConnectorTransactionStore>> connectors;
public:
    TransactionStore(unsigned int nConnectors, std::shared_ptr<FilesystemAdapter> filesystem, const ProtocolVersion& version=VER_1_6_J);

    bool commit(ITransaction *transaction);

    std::shared_ptr<ITransaction> getTransaction(unsigned int connectorId, unsigned int txNr);
    std::shared_ptr<ITransaction> createTransaction(unsigned int connectorId, unsigned int txNr, bool silent = false);

    bool remove(unsigned int connectorId, unsigned int txNr);
};

}

#endif
