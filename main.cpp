#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <map>
#include <ctime>
#include <algorithm>
#include <random>

class Transaction {
public:
    std::string type;
    std::string category;
    double amount;
    double balanceAfter;
    std::time_t timestamp;

    Transaction(std::string t, double amt, double bal, std::string cat = "Uncategorized")
            : type(t), amount(amt), balanceAfter(bal), category(cat) {
        timestamp = std::time(nullptr);
    }
};

class BankAccount {
private:
    std::string accountHolder;
    long accountNumber;
    double balance;
    double interestRate;
    double overdraftLimit;
    bool isFrozen;
    std::vector<Transaction> transactionHistory;
    std::map<std::string, double> currencies;
    std::string username;
    std::string password;
    bool isLocked;
    double dailyTransactionLimit;
    std::string secureProtocol;
    double loanAmount;
    double loanInterestRate;

public:
    BankAccount(const std::string &name, long number, double initialBalance, double interestRate = 0.0, double overdraftLimit = 0.0,
                const std::string &username = "", const std::string &password = "", double dailyLimit = 1000.00)
            : accountHolder(name), accountNumber(number), balance(initialBalance), interestRate(interestRate), overdraftLimit(overdraftLimit),
              isFrozen(false), username(username), password(password), isLocked(false), dailyTransactionLimit(dailyLimit), secureProtocol("HTTPS"),
              loanAmount(0.0), loanInterestRate(0.0) {
        currencies["USD"] = initialBalance;
    }

    // Authenticate user
    bool authenticate(const std::string &inputUsername, const std::string &inputPassword) {
        return (username == inputUsername && password == inputPassword);
    }

    // Deposit funds into the account
    void deposit(double amount, std::string currency = "USD") {
        if (amount > 0) {
            balance += amount;
            currencies[currency] += amount;
            transactionHistory.emplace_back("Deposit", amount, balance, currency);
            std::cout << "Deposit successful. Current balance: $" << std::fixed << std::setprecision(2) << balance << std::endl;
            notifyTransaction("Deposit", amount);
        } else {
            std::cout << "Invalid amount. Please enter a positive number." << std::endl;
        }
    }

    // Withdraw funds from the account
    void withdraw(double amount, std::string currency = "USD") {
        if (amount > 0 && (balance + overdraftLimit) >= amount && currencies[currency] >= amount && !isLocked) {
            balance -= amount;
            currencies[currency] -= amount;
            transactionHistory.emplace_back("Withdrawal", amount, balance, currency);
            std::cout << "Withdrawal successful. Current balance: $" << std::fixed << std::setprecision(2) << balance << std::endl;
            notifyTransaction("Withdrawal", amount);
        } else {
            std::cout << "Invalid amount, exceeding overdraft limit, insufficient funds in specified currency, or account locked." << std::endl;
        }
    }

    // Categorize a transaction in the transaction history
    void categorizeTransaction(int index, std::string category) {
        if (index >= 0 && static_cast<size_t>(index) < transactionHistory.size()) {
            transactionHistory[index].category = category;
            std::cout << "Transaction categorized successfully." << std::endl;
        } else {
            std::cout << "Invalid transaction index." << std::endl;
        }
    }

    // Reverse a transaction from the transaction history
    void reverseTransaction(int index) {
        if (index >= 0 && static_cast<size_t>(index) < transactionHistory.size()) {
            if (transactionHistory[index].type == "Deposit") {
                balance -= transactionHistory[index].amount;
            } else {
                balance += transactionHistory[index].amount;
            }
            transactionHistory.erase(transactionHistory.begin() + index);
            std::cout << "Transaction reversed successfully." << std::endl;
        } else {
            std::cout << "Invalid transaction index." << std::endl;
        }
    }

    // Freeze the account
    void freezeAccount() {
        isFrozen = true;
        std::cout << "Account frozen." << std::endl;
    }

    // Unfreeze the account
    void unfreezeAccount() {
        isFrozen = false;
        std::cout << "Account unfrozen." << std::endl;
    }

    // Lock the account
    void lockAccount() {
        isLocked = true;
        std::cout << "Account locked." << std::endl;
    }

    // Unlock the account
    void unlockAccount() {
        isLocked = false;
        std::cout << "Account unlocked." << std::endl;
    }

    // Set the daily transaction limit
    void setDailyTransactionLimit(double limit) {
        dailyTransactionLimit = limit;
        std::cout << "Daily transaction limit set to $" << std::fixed << std::setprecision(2) << limit << std::endl;
    }

    // Set the secure communication protocol
    void setSecureProtocol(const std::string &protocol) {
        secureProtocol = protocol;
        std::cout << "Secure communication protocol set to " << protocol << std::endl;
    }

    // Notify user about a transaction
    void notifyTransaction(const std::string &type, double amount) {
        std::cout << "Notification: Transaction of $" << std::fixed << std::setprecision(2) << amount << " " << type << " has been performed." << std::endl;
    }

    // Generate a statement for the account
    void generateStatement() const {
        std::cout << "Statement for Account Number: " << accountNumber << std::endl;
        std::cout << "Account Holder: " << accountHolder << std::endl;
        std::cout << "Balance: $" << std::fixed << std::setprecision(2) << balance << std::endl;
        std::cout << "Transaction History:" << std::endl;
        for (const auto &transaction : transactionHistory) {
            std::cout << "Type: " << transaction.type << ", Amount: $" << transaction.amount
                      << ", Balance: $" << transaction.balanceAfter << ", Category: " << transaction.category << std::endl;
        }
    }

    // Request a loan
    void requestLoan(double amount, double interestRate) {
        if (amount > 0) {
            loanAmount += amount;
            loanInterestRate = interestRate;
            balance += amount;
            transactionHistory.emplace_back("Loan", amount, balance);
            std::cout << "Loan of $" << std::fixed << std::setprecision(2) << amount << " approved with an interest rate of "
                      << interestRate << "%. Current balance: $" << balance << std::endl;
        } else {
            std::cout << "Invalid loan amount. Please enter a positive number." << std::endl;
        }
    }

    // Apply interest on the loan
    void applyInterest() {
        if (loanAmount > 0) {
            double interestAmount = loanAmount * (loanInterestRate / 100.0);
            loanAmount += interestAmount;
            balance += interestAmount;
            transactionHistory.emplace_back("Interest Payment", interestAmount, balance);
            std::cout << "Interest of $" << std::fixed << std::setprecision(2) << interestAmount
                      << " added to the loan. Current balance: $" << balance << std::endl;
        } else {
            std::cout << "No outstanding loan to apply interest." << std::endl;
        }
    }

    // Repay a loan
    void repayLoan(double amount) {
        if (amount > 0 && balance >= amount) {
            balance -= amount;
            loanAmount -= amount;
            transactionHistory.emplace_back("Loan Repayment", amount, balance);
            std::cout << "Loan repayment of $" << std::fixed << std::setprecision(2) << amount
                      << " successful. Current balance: $" << balance << std::endl;
        } else {
            std::cout << "Invalid amount or insufficient balance to repay the loan." << std::endl;
        }
    }

    // Transfer funds to another account
    void transferTo(BankAccount &recipient, double amount) {
        if (!isFrozen && !isLocked && amount > 0 && balance >= amount && recipient.isFrozen == false && recipient.isLocked == false) {
            if ((amount + recipient.dailyTransactionLimit) <= recipient.dailyTransactionLimit) {
                balance -= amount;
                recipient.balance += amount;
                transactionHistory.emplace_back("Transfer", amount, balance);
                recipient.transactionHistory.emplace_back("Received", amount, recipient.balance);
                std::cout << "Transfer of $" << std::fixed << std::setprecision(2) << amount
                          << " to account " << recipient.accountNumber << " successful. Current balance: $" << balance << std::endl;
            } else {
                std::cout << "Exceeds recipient's daily transaction limit." << std::endl;
            }
        } else {
            std::cout << "Invalid amount, insufficient balance, or one of the accounts is frozen or locked." << std::endl;
        }
    }

    // Check account balance
    void checkBalance() const {
        std::cout << "The current balance is: $" << std::fixed << std::setprecision(2) << balance << std::endl;
    }
};

int main() {
    // Create BankAccount objects
    BankAccount userAccount("John Doe", 123456789, 1000.00, 5.0, 500.00, "john_doe", "password123", 1000.00); // 5% interest rate, $500 overdraft limit, $1000 daily transaction limit
    BankAccount creditCardAccount("Credit Card", 987654321, 0.00);

    // Simulate account operations.
    if (userAccount.authenticate("john_doe", "password123")) {
        userAccount.deposit(500.00);
        userAccount.withdraw(200.00);
        userAccount.requestLoan(1000.00, 3.5); // Request loan with 3.5% interest rate
        userAccount.applyInterest();
        userAccount.checkBalance();
        userAccount.freezeAccount();
        userAccount.unfreezeAccount();
        userAccount.setDailyTransactionLimit(1500.00);
        userAccount.setSecureProtocol("TLS");
        userAccount.transferTo(creditCardAccount, 300.00);
        userAccount.generateStatement();
    } else {
        std::cout << "Invalid username or password. Transaction cancelled." << std::endl;
    }

    return 0;
}
