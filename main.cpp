// cl /std:c++20 /EHsc /W4 /O2 main.cpp MarkovNameGenerator.cpp /Fe:NameGenerator.exe
#include "MarkovNameGenerator.h"

#include <iostream>

int wmain()
{
    CMarkovNameGenerator male;
    CMarkovNameGenerator female;
    CMarkovNameGenerator surname;

    // Train models.
     if (!male.Train(L"male-names-list.txt", 3))
         return 1;

     if (!female.Train(L"female-names-list.txt", 3))
         return 1;

     if (!surname.Train(L"surnames-list.txt", 3))
         return 1;

    // Save models.
    male.Save(L"male.model");
    female.Save(L"female.model");
    surname.Save(L"surnames.model");
    
    male.Load(L"male.model");
    female.Load(L"female.model");
    surname.Load(L"surnames.model");

    // Generate names.
    std::wcout << L'\n';

    for (int n=0;n<50;n++)
        std::wcout << female.Generate(L"", 4, 20) << " " << surname.Generate(L"", 5, 50) << L'\n';

    std::wcout << L'\n';

    for (int n=0;n<50;n++)
        std::wcout << male.Generate(L"", 4, 20) << " " << surname.Generate(L"", 5, 50) << L'\n';

    return 0;
}