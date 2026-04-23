#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace std;

const int MAX_BLOCK_SIZE = 320;
const int MAX_STRING_LEN = 65;

struct Account {
    char userID[31];
    char password[31];
    char username[31];
    int privilege;
    
    Account() : privilege(0) {
        memset(userID, 0, sizeof(userID));
        memset(password, 0, sizeof(password));
        memset(username, 0, sizeof(username));
    }
};

struct Book {
    char ISBN[21];
    char name[61];
    char author[61];
    char keyword[61];
    double price;
    int quantity;
    
    Book() : price(0), quantity(0) {
        memset(ISBN, 0, sizeof(ISBN));
        memset(name, 0, sizeof(name));
        memset(author, 0, sizeof(author));
        memset(keyword, 0, sizeof(keyword));
    }
};

struct IndexPair {
    char key[MAX_STRING_LEN];
    int offset;
    
    bool operator<(const IndexPair& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return offset < other.offset;
    }
};

template<typename T>
class FileStorage {
private:
    string filename;
    fstream file;
    int count;
    
public:
    FileStorage(const string& fname) : filename(fname), count(0) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file) {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            file.write(reinterpret_cast<char*>(&count), sizeof(int));
        } else {
            file.read(reinterpret_cast<char*>(&count), sizeof(int));
        }
    }
    
    ~FileStorage() {
        if (file.is_open()) {
            file.seekp(0);
            file.write(reinterpret_cast<char*>(&count), sizeof(int));
            file.close();
        }
    }
    
    int add(const T& data) {
        int offset = count++;
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&count), sizeof(int));
        file.seekp(sizeof(int) + offset * sizeof(T));
        file.write(reinterpret_cast<const char*>(&data), sizeof(T));
        file.flush();
        return offset;
    }
    
    bool read(int offset, T& data) {
        if (offset < 0 || offset >= count) return false;
        file.seekg(sizeof(int) + offset * sizeof(T));
        file.read(reinterpret_cast<char*>(&data), sizeof(T));
        return true;
    }
    
    bool update(int offset, const T& data) {
        if (offset < 0 || offset >= count) return false;
        file.seekp(sizeof(int) + offset * sizeof(T));
        file.write(reinterpret_cast<const char*>(&data), sizeof(T));
        file.flush();
        return true;
    }
    
    int getCount() { return count; }
    
    void readAll(vector<T>& result) {
        result.clear();
        for (int i = 0; i < count; i++) {
            T data;
            read(i, data);
            result.push_back(data);
        }
    }
};

class UnrolledLinkedList {
private:
    string filename;
    fstream file;
    vector<IndexPair> cache;
    bool modified;
    
public:
    UnrolledLinkedList(const string& fname) : filename(fname), modified(false) {
        file.open(filename, ios::in | ios::binary);
        if (file) {
            IndexPair pair;
            while (file.read(reinterpret_cast<char*>(&pair), sizeof(IndexPair))) {
                cache.push_back(pair);
            }
            file.close();
        }
    }
    
    ~UnrolledLinkedList() {
        save();
    }
    
    void save() {
        if (!modified) return;
        file.open(filename, ios::out | ios::binary | ios::trunc);
        for (const auto& pair : cache) {
            file.write(reinterpret_cast<const char*>(&pair), sizeof(IndexPair));
        }
        file.close();
        modified = false;
    }
    
    void insert(const char* key, int offset) {
        IndexPair pair;
        strncpy(pair.key, key, MAX_STRING_LEN - 1);
        pair.offset = offset;
        
        auto it = lower_bound(cache.begin(), cache.end(), pair);
        if (it == cache.end() || strcmp(it->key, key) != 0 || it->offset != offset) {
            cache.insert(it, pair);
            modified = true;
        }
    }
    
    void remove(const char* key, int offset) {
        IndexPair pair;
        strncpy(pair.key, key, MAX_STRING_LEN - 1);
        pair.offset = offset;
        
        auto it = lower_bound(cache.begin(), cache.end(), pair);
        if (it != cache.end() && strcmp(it->key, key) == 0 && it->offset == offset) {
            cache.erase(it);
            modified = true;
        }
    }
    
    vector<int> find(const char* key) {
        vector<int> result;
        IndexPair pair;
        strncpy(pair.key, key, MAX_STRING_LEN - 1);
        pair.offset = -1;
        
        auto it = lower_bound(cache.begin(), cache.end(), pair);
        while (it != cache.end() && strcmp(it->key, key) == 0) {
            result.push_back(it->offset);
            ++it;
        }
        return result;
    }
    
    vector<int> findAll() {
        vector<int> result;
        for (const auto& pair : cache) {
            result.push_back(pair.offset);
        }
        return result;
    }
};

class BookstoreSystem {
private:
    FileStorage<Account>* accountStorage;
    FileStorage<Book>* bookStorage;
    UnrolledLinkedList* accountIndex;
    UnrolledLinkedList* isbnIndex;
    UnrolledLinkedList* nameIndex;
    UnrolledLinkedList* authorIndex;
    UnrolledLinkedList* keywordIndex;
    
    struct LoginState {
        int accountOffset;
        int selectedBook;
    };
    
    vector<LoginState> loginStack;
    vector<double> financeLog;
    vector<string> operationLog;
    
    fstream financeFile;
    fstream logFile;
    
public:
    BookstoreSystem() {
        accountStorage = new FileStorage<Account>("accounts.dat");
        bookStorage = new FileStorage<Book>("books.dat");
        accountIndex = new UnrolledLinkedList("account_index.dat");
        isbnIndex = new UnrolledLinkedList("isbn_index.dat");
        nameIndex = new UnrolledLinkedList("name_index.dat");
        authorIndex = new UnrolledLinkedList("author_index.dat");
        keywordIndex = new UnrolledLinkedList("keyword_index.dat");
        
        if (accountStorage->getCount() == 0) {
            Account root;
            strcpy(root.userID, "root");
            strcpy(root.password, "sjtu");
            strcpy(root.username, "root");
            root.privilege = 7;
            int offset = accountStorage->add(root);
            accountIndex->insert(root.userID, offset);
        }
        
        financeFile.open("finance.log", ios::in | ios::binary);
        if (financeFile) {
            double val;
            while (financeFile.read(reinterpret_cast<char*>(&val), sizeof(double))) {
                financeLog.push_back(val);
            }
            financeFile.close();
        }
        
        logFile.open("operation.log", ios::in);
        if (logFile) {
            string line;
            while (getline(logFile, line)) {
                operationLog.push_back(line);
            }
            logFile.close();
        }
    }
    
    ~BookstoreSystem() {
        financeFile.open("finance.log", ios::out | ios::binary | ios::trunc);
        for (double val : financeLog) {
            financeFile.write(reinterpret_cast<const char*>(&val), sizeof(double));
        }
        financeFile.close();
        
        logFile.open("operation.log", ios::out | ios::trunc);
        for (const string& line : operationLog) {
            logFile << line << "\n";
        }
        logFile.close();
        
        delete accountStorage;
        delete bookStorage;
        delete accountIndex;
        delete isbnIndex;
        delete nameIndex;
        delete authorIndex;
        delete keywordIndex;
    }
    
    int getCurrentPrivilege() {
        if (loginStack.empty()) return 0;
        Account acc;
        accountStorage->read(loginStack.back().accountOffset, acc);
        return acc.privilege;
    }
    
    void logOperation(const string& op) {
        operationLog.push_back(op);
    }
    
    bool su(const string& userID, const string& password) {
        vector<int> offsets = accountIndex->find(userID.c_str());
        if (offsets.empty()) return false;
        
        Account acc;
        accountStorage->read(offsets[0], acc);
        
        if (!password.empty()) {
            if (strcmp(acc.password, password.c_str()) != 0) return false;
        } else {
            if (getCurrentPrivilege() <= acc.privilege) return false;
        }
        
        LoginState state;
        state.accountOffset = offsets[0];
        state.selectedBook = -1;
        loginStack.push_back(state);
        
        logOperation("su " + userID);
        return true;
    }
    
    bool logout() {
        if (loginStack.empty()) return false;
        loginStack.pop_back();
        logOperation("logout");
        return true;
    }
    
    bool registerAccount(const string& userID, const string& password, const string& username) {
        if (!accountIndex->find(userID.c_str()).empty()) return false;
        
        Account acc;
        strcpy(acc.userID, userID.c_str());
        strcpy(acc.password, password.c_str());
        strcpy(acc.username, username.c_str());
        acc.privilege = 1;
        
        int offset = accountStorage->add(acc);
        accountIndex->insert(acc.userID, offset);
        
        logOperation("register " + userID);
        return true;
    }
    
    bool passwd(const string& userID, const string& currentPassword, const string& newPassword) {
        if (getCurrentPrivilege() < 1) return false;
        
        vector<int> offsets = accountIndex->find(userID.c_str());
        if (offsets.empty()) return false;
        
        Account acc;
        accountStorage->read(offsets[0], acc);
        
        if (!currentPassword.empty()) {
            if (strcmp(acc.password, currentPassword.c_str()) != 0) return false;
        } else {
            if (getCurrentPrivilege() != 7) return false;
        }
        
        strcpy(acc.password, newPassword.c_str());
        accountStorage->update(offsets[0], acc);
        
        logOperation("passwd " + userID);
        return true;
    }
    
    bool useradd(const string& userID, const string& password, int privilege, const string& username) {
        if (getCurrentPrivilege() < 3) return false;
        if (privilege >= getCurrentPrivilege()) return false;
        if (!accountIndex->find(userID.c_str()).empty()) return false;
        
        Account acc;
        strcpy(acc.userID, userID.c_str());
        strcpy(acc.password, password.c_str());
        strcpy(acc.username, username.c_str());
        acc.privilege = privilege;
        
        int offset = accountStorage->add(acc);
        accountIndex->insert(acc.userID, offset);
        
        logOperation("useradd " + userID);
        return true;
    }
    
    bool deleteAccount(const string& userID) {
        if (getCurrentPrivilege() != 7) return false;
        
        vector<int> offsets = accountIndex->find(userID.c_str());
        if (offsets.empty()) return false;
        
        for (const auto& state : loginStack) {
            if (state.accountOffset == offsets[0]) return false;
        }
        
        accountIndex->remove(userID.c_str(), offsets[0]);
        logOperation("delete " + userID);
        return true;
    }
    
    bool show(const string& type, const string& value) {
        if (getCurrentPrivilege() < 1) return false;
        
        vector<int> offsets;
        if (type.empty()) {
            offsets = isbnIndex->findAll();
        } else if (type == "ISBN") {
            offsets = isbnIndex->find(value.c_str());
        } else if (type == "name") {
            offsets = nameIndex->find(value.c_str());
        } else if (type == "author") {
            offsets = authorIndex->find(value.c_str());
        } else if (type == "keyword") {
            offsets = keywordIndex->find(value.c_str());
        }
        
        set<string> uniqueISBNs;
        vector<Book> books;
        for (int offset : offsets) {
            Book book;
            if (bookStorage->read(offset, book)) {
                if (uniqueISBNs.find(book.ISBN) == uniqueISBNs.end()) {
                    uniqueISBNs.insert(book.ISBN);
                    books.push_back(book);
                }
            }
        }
        
        sort(books.begin(), books.end(), [](const Book& a, const Book& b) {
            return strcmp(a.ISBN, b.ISBN) < 0;
        });
        
        if (books.empty()) {
            cout << "\n";
        } else {
            for (const Book& book : books) {
                cout << book.ISBN << "\t" << book.name << "\t" << book.author << "\t" 
                     << book.keyword << "\t" << fixed << setprecision(2) << book.price 
                     << "\t" << book.quantity << "\n";
            }
        }
        
        return true;
    }
    
    bool buy(const string& ISBN, int quantity) {
        if (getCurrentPrivilege() < 1) return false;
        if (quantity <= 0) return false;
        
        vector<int> offsets = isbnIndex->find(ISBN.c_str());
        if (offsets.empty()) return false;
        
        Book book;
        bookStorage->read(offsets[0], book);
        
        if (book.quantity < quantity) return false;
        
        book.quantity -= quantity;
        bookStorage->update(offsets[0], book);
        
        double total = book.price * quantity;
        financeLog.push_back(total);
        
        cout << fixed << setprecision(2) << total << "\n";
        logOperation("buy " + ISBN + " " + to_string(quantity));
        return true;
    }
    
    bool select(const string& ISBN) {
        if (getCurrentPrivilege() < 3) return false;
        if (loginStack.empty()) return false;
        
        vector<int> offsets = isbnIndex->find(ISBN.c_str());
        if (offsets.empty()) {
            Book book;
            strcpy(book.ISBN, ISBN.c_str());
            int offset = bookStorage->add(book);
            isbnIndex->insert(book.ISBN, offset);
            loginStack.back().selectedBook = offset;
        } else {
            loginStack.back().selectedBook = offsets[0];
        }
        
        logOperation("select " + ISBN);
        return true;
    }
    
    bool modify(const map<string, string>& params) {
        if (getCurrentPrivilege() < 3) return false;
        if (loginStack.empty() || loginStack.back().selectedBook == -1) return false;
        
        Book book;
        bookStorage->read(loginStack.back().selectedBook, book);
        
        string oldISBN = book.ISBN;
        
        for (const auto& param : params) {
            if (param.first == "ISBN") {
                if (strcmp(book.ISBN, param.second.c_str()) == 0) return false;
                if (!isbnIndex->find(param.second.c_str()).empty()) return false;
                strcpy(book.ISBN, param.second.c_str());
            } else if (param.first == "name") {
                strcpy(book.name, param.second.c_str());
            } else if (param.first == "author") {
                strcpy(book.author, param.second.c_str());
            } else if (param.first == "keyword") {
                strcpy(book.keyword, param.second.c_str());
            } else if (param.first == "price") {
                book.price = stod(param.second);
            }
        }
        
        if (strcmp(oldISBN.c_str(), book.ISBN) != 0) {
            isbnIndex->remove(oldISBN.c_str(), loginStack.back().selectedBook);
            isbnIndex->insert(book.ISBN, loginStack.back().selectedBook);
        }
        
        bookStorage->update(loginStack.back().selectedBook, book);
        logOperation("modify");
        return true;
    }
    
    bool import(int quantity, double totalCost) {
        if (getCurrentPrivilege() < 3) return false;
        if (loginStack.empty() || loginStack.back().selectedBook == -1) return false;
        if (quantity <= 0 || totalCost <= 0) return false;
        
        Book book;
        bookStorage->read(loginStack.back().selectedBook, book);
        book.quantity += quantity;
        bookStorage->update(loginStack.back().selectedBook, book);
        
        financeLog.push_back(-totalCost);
        
        logOperation("import " + to_string(quantity));
        return true;
    }
    
    bool showFinance(int count) {
        if (getCurrentPrivilege() != 7) return false;
        
        if (count == 0) {
            cout << "\n";
            return true;
        }
        
        if (count < 0 || count > (int)financeLog.size()) return false;
        
        double income = 0, expenditure = 0;
        int start = financeLog.size() - count;
        for (int i = start; i < (int)financeLog.size(); i++) {
            if (financeLog[i] > 0) income += financeLog[i];
            else expenditure += -financeLog[i];
        }
        
        cout << "+ " << fixed << setprecision(2) << income << " - " << expenditure << "\n";
        return true;
    }
    
    bool showFinanceAll() {
        if (getCurrentPrivilege() != 7) return false;
        
        double income = 0, expenditure = 0;
        for (double val : financeLog) {
            if (val > 0) income += val;
            else expenditure += -val;
        }
        
        cout << "+ " << fixed << setprecision(2) << income << " - " << expenditure << "\n";
        return true;
    }
    
    bool log() {
        if (getCurrentPrivilege() != 7) return false;
        
        for (const string& line : operationLog) {
            cout << line << "\n";
        }
        return true;
    }
    
    bool reportFinance() {
        if (getCurrentPrivilege() != 7) return false;
        cout << "Finance Report\n";
        return true;
    }
    
    bool reportEmployee() {
        if (getCurrentPrivilege() != 7) return false;
        cout << "Employee Report\n";
        return true;
    }
};

vector<string> splitCommand(const string& line) {
    vector<string> tokens;
    string token;
    bool inQuotes = false;
    
    for (size_t i = 0; i < line.length(); i++) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
            token += c;
        } else if (c == ' ' && !inQuotes) {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    
    if (!token.empty()) {
        tokens.push_back(token);
    }
    
    return tokens;
}

string removeQuotes(const string& s) {
    if (s.length() >= 2 && s[0] == '"' && s[s.length()-1] == '"') {
        return s.substr(1, s.length()-2);
    }
    return s;
}

int main() {
    BookstoreSystem system;
    string line;
    
    while (getline(cin, line)) {
        if (line.empty() || line.find_first_not_of(' ') == string::npos) {
            continue;
        }
        
        vector<string> tokens = splitCommand(line);
        if (tokens.empty()) continue;
        
        string cmd = tokens[0];
        bool success = false;
        
        try {
            if (cmd == "quit" || cmd == "exit") {
                break;
            } else if (cmd == "su") {
                if (tokens.size() == 2) {
                    success = system.su(tokens[1], "");
                } else if (tokens.size() == 3) {
                    success = system.su(tokens[1], tokens[2]);
                }
            } else if (cmd == "logout") {
                success = system.logout();
            } else if (cmd == "register") {
                if (tokens.size() == 4) {
                    success = system.registerAccount(tokens[1], tokens[2], tokens[3]);
                }
            } else if (cmd == "passwd") {
                if (tokens.size() == 3) {
                    success = system.passwd(tokens[1], "", tokens[2]);
                } else if (tokens.size() == 4) {
                    success = system.passwd(tokens[1], tokens[2], tokens[3]);
                }
            } else if (cmd == "useradd") {
                if (tokens.size() == 5) {
                    int priv = stoi(tokens[3]);
                    success = system.useradd(tokens[1], tokens[2], priv, tokens[4]);
                }
            } else if (cmd == "delete") {
                if (tokens.size() == 2) {
                    success = system.deleteAccount(tokens[1]);
                }
            } else if (cmd == "show") {
                if (tokens.size() == 1) {
                    success = system.show("", "");
                } else if (tokens[1] == "finance") {
                    if (tokens.size() == 2) {
                        success = system.showFinanceAll();
                    } else if (tokens.size() == 3) {
                        int count = stoi(tokens[2]);
                        success = system.showFinance(count);
                    }
                } else {
                    string param = tokens[1];
                    size_t eqPos = param.find('=');
                    if (eqPos != string::npos) {
                        string type = param.substr(1, eqPos - 1);
                        string value = param.substr(eqPos + 1);
                        value = removeQuotes(value);
                        success = system.show(type, value);
                    }
                }
            } else if (cmd == "buy") {
                if (tokens.size() == 3) {
                    int quantity = stoi(tokens[2]);
                    success = system.buy(tokens[1], quantity);
                }
            } else if (cmd == "select") {
                if (tokens.size() == 2) {
                    success = system.select(tokens[1]);
                }
            } else if (cmd == "modify") {
                map<string, string> params;
                for (size_t i = 1; i < tokens.size(); i++) {
                    size_t eqPos = tokens[i].find('=');
                    if (eqPos != string::npos) {
                        string key = tokens[i].substr(1, eqPos - 1);
                        string value = tokens[i].substr(eqPos + 1);
                        value = removeQuotes(value);
                        params[key] = value;
                    }
                }
                success = system.modify(params);
            } else if (cmd == "import") {
                if (tokens.size() == 3) {
                    int quantity = stoi(tokens[1]);
                    double totalCost = stod(tokens[2]);
                    success = system.import(quantity, totalCost);
                }
            } else if (cmd == "log") {
                success = system.log();
            } else if (cmd == "report") {
                if (tokens.size() == 2) {
                    if (tokens[1] == "finance") {
                        success = system.reportFinance();
                    } else if (tokens[1] == "employee") {
                        success = system.reportEmployee();
                    }
                }
            }
            
            if (!success && cmd != "quit" && cmd != "exit") {
                cout << "Invalid\n";
            }
        } catch (...) {
            cout << "Invalid\n";
        }
    }
    
    return 0;
}
