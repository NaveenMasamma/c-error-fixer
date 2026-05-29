#include <gmock/gmock.h>
#include "IFixEngine.h"

class MockFixEngine : public IFixEngine {
public:
    MOCK_METHOD(bool, canHandle, (const std::string&), (const, override));
    MOCK_METHOD(std::vector<FixSuggestion>, generateSuggestions, 
                (const CompilerError&, const std::vector<std::string>&, 
                 CodeAnalyzer&, ErrorPatternDB&), (override));
};