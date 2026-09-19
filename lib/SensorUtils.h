#pragma once
#include <functional>

namespace SensorUtils {

    constexpr uint8_t UNUSED_MUX = 0xFF;

    using FunctionCallback = void(*)();

    struct FunctionNode {
        // Node structure for function linked list

        FunctionCallback function;
        FunctionNode* next;

        // CONSTRUCTOR: Intake the function, next pointer will be added in
        // subsequent declaration
        FunctionNode(FunctionCallback func) : function(func){};
    };

    FunctionNode* linkFunctionCallback (std::initializer_list<FunctionCallback>& functionList) {
        // Create a linked-list for functions being used

        if (functionList.size() == 0) { return nullptr; }

        FunctionNode* head = nullptr;
        FunctionNode* tail;

        for (const FunctionCallback& function : functionList) {
            FunctionNode* newNode = new FunctionNode(function);
            if (head == nullptr){
                head = newNode;
                tail = newNode;
            } else {
                tail->next = newNode;
                tail = newNode;
            }
        }

        return head;
    }
}