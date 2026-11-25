# Markov Name Generator

A small C++20 name generator for Windows that uses a character-level
Markov model to create first names and surnames.

Train separate generator instances from text files containing names,
save each trained model to a binary `.model` file, and load those models
later to generate names without retraining.

The generator can be used for male names, female names, surnames, or any
other name list. First names and surnames can be generated independently
and combined by the calling application.

## Features

-   Character-level Markov name generation.
-   Separate models can be trained with different Markov orders.
-   Loads name lists from UTF-8 text files, with a fallback to the
    Windows ANSI code page.
-   Accepts names separated by CR, LF, or CRLF line endings.
-   Saves and loads binary model files.
-   Supports an optional starting prefix and minimum/maximum generated
    name lengths.
-   Applies a light quality filter to reject obvious repeated character
    patterns.
-   Capitalises the first character and lowercases the remaining
    characters in generated names.

## Requirements

-   Windows
-   Visual Studio 2022 C++ compiler (MSVC)
-   C++20 support

## Files

Typical project files:

``` text
main.cpp
MarkovNameGenerator.h
MarkovNameGenerator.cpp
male-names-list.txt
female-names-list.txt
surnames-list.txt
```

The `.txt` files are training data: one name per line. Names can be
uppercase or mixed case; generated output is formatted with an initial
capital and the remaining letters lowercase.

The `.model` files are binary files created by the generator. They
contain the trained Markov data and are intended to be loaded by this
application.

## Compile

Open an **x64 Native Tools Command Prompt for VS 2022** (or the
appropriate Developer Command Prompt), change to the project directory,
and run:

``` bat
cl /std:c++20 /EHsc /W4 /O2 main.cpp MarkovNameGenerator.cpp /Fe:NameGenerator.exe
```

If your project uses additional source files, include them in the
command as well.

## Interface

The class is declared in `MarkovNameGenerator.h`:

``` cpp
class CMarkovNameGenerator
{
public:
    CMarkovNameGenerator();

    bool Train(const std::wstring& filename, unsigned int order = 3);
    bool Save(const std::wstring& filename) const;
    bool Load(const std::wstring& filename);

    std::wstring Generate(
        const std::wstring& start = L"",
        unsigned int minLength = 4,
        unsigned int maxLength = 12);

private:
    // Markov model and random-number generator
};
```

### Constructor

``` cpp
CMarkovNameGenerator();
```

Constructs a generator and seeds its random-number generator. Seeding
happens once per instance, so generation works whether the model is
trained in the current run or loaded from a saved file.

### Train

``` cpp
bool Train(const std::wstring& filename, unsigned int order = 3);
```

Reads a text file and builds the Markov model.

-   `filename`: path to the training text file.
-   `order`: number of preceding characters used as the context when
    choosing the next character. Defaults to `3`.

Returns `true` if training succeeds, otherwise `false`.

### Save

``` cpp
bool Save(const std::wstring& filename) const;
```

Saves the trained model to a binary file. Returns `true` on success.

### Load

``` cpp
bool Load(const std::wstring& filename);
```

Loads a previously saved binary model. Returns `true` on success.

### Generate

``` cpp
std::wstring Generate(
    const std::wstring& start = L"",
    unsigned int minLength = 4,
    unsigned int maxLength = 12);
```

Generates a name using the current model.

-   `start`: optional prefix that the generated name must begin with (UPPERCASE).
-   `minLength`: minimum total name length, including the prefix.
-   `maxLength`: maximum total name length, including the prefix.

Returns the generated name, or an empty string if it cannot generate an
acceptable result within its attempts.

## Example usage

### Train and save a model

``` cpp
#include "MarkovNameGenerator.h"

int wmain()
{
    CMarkovNameGenerator female;

    if (!female.Train(L"female-names-list.txt", 3))
        return 1;

    if (!female.Save(L"female.model"))
        return 2;

    return 0;
}
```

### Load a model and generate names

``` cpp
#include "MarkovNameGenerator.h"

#include <iostream>

int wmain()
{
    CMarkovNameGenerator female;

    if (!female.Load(L"female.model"))
        return 1;

    for (int i = 0; i < 20; ++i)
    {
        const std::wstring name = female.Generate();

        if (!name.empty())
            std::wcout << name << L'\n';
    }

    return 0;
}
```

### Use separate models for first names and surnames

Train or load each model independently. Each instance retains its own
Markov order and random-number generator.

``` cpp
#include "MarkovNameGenerator.h"

#include <iostream>

int wmain()
{
    CMarkovNameGenerator femaleNames;
    CMarkovNameGenerator surnames;

    if (!femaleNames.Load(L"female.model"))
        return 1;

    if (!surnames.Load(L"surnames.model"))
        return 2;

    for (int i = 0; i < 20; ++i)
    {
        const std::wstring firstName = femaleNames.Generate();
        const std::wstring surname = surnames.Generate();

        if (!firstName.empty() && !surname.empty())
            std::wcout << firstName << L' ' << surname << L'\n';
    }

    return 0;
}
```

## Markov order and its effect on the output

The Markov order controls how many preceding characters the model
considers when selecting the next character. It has a noticeable effect
on the style and variety of generated names.

-   **Lower order (for example, 1 or 2):** Uses less context. It tends
    to produce more varied combinations, but names can sound less
    consistent or contain less natural character sequences.
-   **Medium order (for example, 3 or 4):** Balances variety with
    recognisable name patterns. This is a useful starting point for many
    name lists.
-   **Higher order (for example, 5 or 6):** Uses more context from the
    training names. It can preserve longer familiar sequences and
    produce names that resemble the source data more closely, but may
    reduce variety or make generation fail more often with a small
    training list.

The best order depends on the size and variety of the training data.
Experiment with a few values and compare the results. A larger training
list generally gives higher orders more material to learn from.

The order is stored in the `.model` file, so `Load()` restores the order
used when that model was trained. You do not need to pass the order to
`Generate()`.

## Quality filter

The generator applies a deliberately light filter to generated names. It
rejects obvious repeated patterns, including:

-   Three identical characters in a row, such as `aaa`.
-   A one-, two-, or three-character sequence repeated three times
    consecutively, such as `ananana` or `barbarbar`.

When a candidate is rejected, the generator tries again, up to its
internal attempt limit. If it cannot produce an acceptable name,
`Generate()` returns an empty string.

The filter is intended to catch obvious Markov artefacts without
rejecting unusual but interesting invented names.

## Notes

-   Train a model again and save it whenever its source name list
    changes.
-   Loading a `.model` file avoids the work of decoding the text file
    and rebuilding the model at application startup.
-   Keep the `.model` files generated by the same version of the code.
    If the internal start/end markers or binary format are changed,
    retrain and save the models again.
-   The model is stored as binary data using Windows `wchar_t`
    characters. It is not intended to be a human-readable text file.
-   The start and end markers are internal model markers; they are not
    included in generated names.
