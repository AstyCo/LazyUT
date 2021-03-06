#ifndef FLATBUFFERS_EXTENSIONS_HPP
#define FLATBUFFERS_EXTENSIONS_HPP

#include "extensions/help_functions.hpp"
#include "flatbuffers_schemes/file_tree_generated.h"
#include "types/file_tree.hpp"

namespace FileTreeFunc {

void deserialize(FileTree &tree, const SplittedPath &sp);
void serialize(const FileTree &tree, const SplittedPath &fileName);

template < typename FT, typename T >
void copyVector(const FT &flatVector, T &v);

template < typename TContainer >
void copyListSplitted(const LazyUT::ListSplitted &fv, TContainer &v);

} // namespace FileTreeFunc

#endif // FLATBUFFERS_EXTENSIONS_HPP
