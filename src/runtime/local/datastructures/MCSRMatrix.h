/*
 * Copyright 2021 The DAPHNE Consortium
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


 #pragma once

 #include <runtime/local/datastructures/DataObjectFactory.h>
 #include <runtime/local/datastructures/Matrix.h>
 #include <runtime/local/datastructures/ValueTypeUtils.h>

 #include <algorithm>
 #include <iostream>
 #include <memory>
 #include <cmath>
 #include <cassert>
 #include <cstddef>
 #include <cstring>

 template<typename ValueType>
 class MCSRMatrix : public Matrix<ValueType>{

   using Matrix<ValueType>::numRows;
   using Matrix<ValueType>::numCols;
   size_t numRowsAllocated;
   bool isRowAllocatedBefore;
   size_t maxNumNonZeros;
   std::shared_ptr<std::shared_ptr<ValueType>[]> values;
   std::shared_ptr<std::shared_ptr<size_t>[]> colIdxs;
   std::shared_ptr<size_t[]> valueSizes;
   std::shared_ptr<size_t[]> allocatedRowSizes;


   // Grant DataObjectFactory access to the private constructors and
   // destructors.
   template<class DataType, typename ... ArgTypes>
   friend DataType * DataObjectFactory::create(ArgTypes ...);
   template<class DataType>
   friend void DataObjectFactory::destroy(const DataType * obj);


   MCSRMatrix(size_t maxNumRows, size_t numCols, size_t maxNumNonZeros, bool zero) :
        Matrix<ValueType>(maxNumRows, numCols),
        numRowsAllocated(maxNumRows),
        isRowAllocatedBefore(false),
        maxNumNonZeros(maxNumNonZeros),
        values(new std::shared_ptr<ValueType>[numRows], std::default_delete<std::shared_ptr<ValueType>[]>()),
        colIdxs(new std::shared_ptr<size_t>[numRows], std::default_delete<std::shared_ptr<size_t>[]>()),
        valueSizes(new size_t[numRows], std::default_delete<size_t[]>()),
        allocatedRowSizes(new size_t[numRows], std::default_delete<size_t[]>())
    {
        assert(numRows != 0 && "Number of rows must not be zero.");
        size_t baselineRowSize = std::ceil(1.1 * maxNumNonZeros / numRows);
        for(size_t i = 0; i<numRows; i++){
          values.get()[i] = std::shared_ptr<ValueType>(new ValueType[baselineRowSize], std::default_delete<ValueType[]>());
          colIdxs.get()[i] = std::shared_ptr<size_t>(new size_t[baselineRowSize], std::default_delete<size_t[]>());
          if(zero){
            memset(values.get()[i].get(), 0, baselineRowSize * sizeof(ValueType));
            memset(colIdxs.get()[i].get(), 0, baselineRowSize * sizeof(size_t));
          }
          allocatedRowSizes.get()[i] = baselineRowSize;
        }
        if(zero){
          memset(valueSizes.get(), 0, numRows * sizeof(size_t));
        }

    }


    MCSRMatrix(const MCSRMatrix<ValueType> * src, size_t rowLowerIncl, size_t rowUpperExcl) :
        Matrix<ValueType>(rowUpperExcl - rowLowerIncl, src->numCols),
        numRowsAllocated(src->numRowsAllocated - rowLowerIncl),
        isRowAllocatedBefore(rowLowerIncl > 0)
    {
      maxNumNonZeros = src->maxNumNonZeros;
      values = std::shared_ptr<std::shared_ptr<ValueType>[]>(src->values, src->values.get() + rowLowerIncl);
      colIdxs = std::shared_ptr<std::shared_ptr<size_t>[]>(src->colIdxs, src->colIdxs.get() + rowLowerIncl);
      valueSizes = std::shared_ptr<size_t[]>(src->valueSizes, src->valueSizes.get() + rowLowerIncl);
      allocatedRowSizes = std::shared_ptr<size_t[]>(src->allocatedRowSizes, src->allocatedRowSizes.get() + rowLowerIncl);

    }

    virtual ~MCSRMatrix() {
        // nothing to do
    }

    void reallocateRow(size_t rowIdx) {
      // 1. Determine the new size for the row.
      const float growthFactor = 1.5;
      size_t currentSize = allocatedRowSizes.get()[rowIdx];
      size_t newSize = static_cast<size_t>(currentSize * growthFactor);

      // 2. Allocate new arrays.
      std::shared_ptr<ValueType> newRowValues(new ValueType[newSize], std::default_delete<ValueType[]>());
      std::shared_ptr<size_t> newRowColIdxs(new size_t[newSize], std::default_delete<size_t[]>());

      // 3. Copy old data.
      memcpy(newRowValues.get(), values.get()[rowIdx].get(), currentSize * sizeof(ValueType));
      memcpy(newRowColIdxs.get(), colIdxs.get()[rowIdx].get(), currentSize * sizeof(size_t));

      // Initialize the rest of the new space with zeros.
      memset(newRowValues.get() + currentSize, 0, (newSize - currentSize) * sizeof(ValueType));
      memset(newRowColIdxs.get() + currentSize, 0, (newSize - currentSize) * sizeof(size_t));

      // 4. Update the pointers.
      values.get()[rowIdx] = newRowValues;
      colIdxs.get()[rowIdx] = newRowColIdxs;

      // 5. Update the allocatedRowSizes.
      allocatedRowSizes.get()[rowIdx] = newSize;
    }


public:

  size_t getNumRows() const{
    return numRows;
  }

  size_t getNumCols() const{
    return numCols;
  }

  size_t * getAllocatedRowSizes() const {
    return allocatedRowSizes.get();
  }

  size_t getMaxNumNonZeros() const {
      return maxNumNonZeros;
  }

  size_t getNumNonZeros() const {
      size_t total = 0;
      for(size_t i = 0; i<numRows; i++){
        total += valueSizes.get()[i];
      }
      return total;
  }

  size_t getNumNonZeros(size_t rowIdx) const {
      assert((rowIdx < numRows) && "rowIdx is out of bounds");
      return valueSizes.get()[rowIdx];
  }

  size_t * getAllNumNonZeros() const {
    return valueSizes.get();
  }

  ValueType * getValues(size_t rowIdx) {
      return values.get()[rowIdx].get();
  }

  const ValueType * getValues(size_t rowIdx) const {
      return values.get()[rowIdx].get();
  }

  size_t * getColIdxs(size_t rowIdx) {
      assert((rowIdx < numRows) && "rowIdx is out of bounds");
      return colIdxs.get()[rowIdx].get();
  }

  const size_t* getColIdxs(size_t rowIdx) const {
    assert((rowIdx < numRows) && "rowIdx is out of bounds");
    return colIdxs.get()[rowIdx].get();
}

  ValueType get(size_t rowIdx, size_t colIdx) const override {
    // Check if row index is valid
    assert(rowIdx < numRows && "rowIdx is out of bounds");

    // Get the columns and values for the specified row
    size_t* currentColIdxs = colIdxs.get()[rowIdx].get();
    ValueType* currentValues = values.get()[rowIdx].get();

    // The length of the current row's arrays (i.e., number of non-zero elements in this row)
    size_t length = valueSizes.get()[rowIdx];

    // Search for the column index in the current row's colIdxs
    for(size_t i = 0; i < length; i++) {
        if (currentColIdxs[i] == colIdx) {
            return currentValues[i];  // Return the value if found
        }
    }

    // If column index wasn't found, the value is zero
    return static_cast<ValueType>(0);
  }

  void set(size_t rowIdx, size_t colIdx, ValueType value) override{
    assert(rowIdx < numRows && colIdx < numCols && "Indices out of bounds");

    // Retrieve the row's arrays and its current size.
    ValueType* rowValues = values.get()[rowIdx].get();
    size_t* rowColIdxs = colIdxs.get()[rowIdx].get();
    size_t rowSize = valueSizes.get()[rowIdx];

    // If the value is zero and previously was non-zero, remove it.
    if (value == 0) {
        bool found = false;
        for (size_t i = 0; i < rowSize; i++) {
            if (rowColIdxs[i] == colIdx) {
                found = true;
                // Shift the remaining values and column indices.
                for (size_t j = i; j < rowSize - 1; j++) {
                    rowValues[j] = rowValues[j + 1];
                    rowColIdxs[j] = rowColIdxs[j + 1];
                }
                // Reset the last (non-active) value and column index.
                rowValues[rowSize - 1] = 0;
                rowColIdxs[rowSize - 1] = 0;

                valueSizes.get()[rowIdx]--;
                break;
            }
        }
        if (!found) {
            // The value was already zero, nothing to do.
            return;
        }
    } else { // The value is non-zero
        bool updated = false;
        for (size_t i = 0; i < rowSize; i++) {
            if (rowColIdxs[i] == colIdx) {
                // Update the existing value.
                rowValues[i] = value;
                updated = true;
                break;
            }
        }

        if (!updated) {
            // The value is new, ensure we have space to add it.
            if (rowSize >= allocatedRowSizes.get()[rowIdx]) {
                // Reallocation logic...
                // (Increase the allocated space and copy the old values)
                reallocateRow(rowIdx);
                // Also adjust the row pointers after reallocation.
                rowValues = values.get()[rowIdx].get();
                rowColIdxs = colIdxs.get()[rowIdx].get();
            }
            // Append the new value and column index at the end.
            rowValues[rowSize] = value;
            rowColIdxs[rowSize] = colIdx;

            // Increment the size for this row.
            valueSizes.get()[rowIdx]++;
        }
    }
  }

  void prepareAppend() override {
      // Not needed for MCSR
  }

  void append(size_t rowIdx, size_t colIdx, ValueType value) override {
    assert(rowIdx < numRows && colIdx < numCols && "Indices out of bounds");

    // If the value is zero, just return.
    if (value == 0) return;

    ValueType* rowValues = values.get()[rowIdx].get();
    size_t* rowColIdxs = colIdxs.get()[rowIdx].get();
    size_t rowSize = valueSizes.get()[rowIdx];

    // If we've used up all the allocated space, reallocate more memory.
    if (rowSize >= allocatedRowSizes.get()[rowIdx]) {
        // Reallocation logic...
        reallocateRow(rowIdx);

        // Also adjust the row pointers after reallocation.
        rowValues = values.get()[rowIdx].get();
        rowColIdxs = colIdxs.get()[rowIdx].get();
    }

    // Find the right position to insert the new value.
    size_t position = 0;
    while (position < rowSize && rowColIdxs[position] < colIdx) {
        position++;
    }

    // Shift the values and column indices to the right from the found position.
    for (size_t i = rowSize; i > position; i--) {
        rowValues[i] = rowValues[i - 1];
        rowColIdxs[i] = rowColIdxs[i - 1];
    }

    // Insert the new value and its column index.
    rowValues[position] = value;
    rowColIdxs[position] = colIdx;

    // Increase the size for this row.
    valueSizes.get()[rowIdx]++;
  }



  void finishAppend() override {
    // Not needed for MCSR
  }

  void printValue(std::ostream & os, ValueType val) const {
    switch (ValueTypeUtils::codeFor<ValueType>) {
        case ValueTypeCode::SI8 : os << static_cast<int32_t>(val); break;
        case ValueTypeCode::UI8 : os << static_cast<uint32_t>(val); break;
        default : os << val; break;
    }
  }

  void print(std::ostream & os) const override {
      os << "MCSRMatrix(" << numRows << 'x' << numCols << ", "
         << ValueTypeUtils::cppNameFor<ValueType> << ')' << std::endl;

      ValueType * oneRow = new ValueType[numCols];
      for (size_t r = 0; r < numRows; r++) {
          memset(oneRow, 0, numCols * sizeof(ValueType));

          const size_t rowNumNonZeros = valueSizes.get()[r];
          const size_t * rowColIdxs = colIdxs.get()[r].get();
          const ValueType * rowValues = values.get()[r].get();

          for(size_t i = 0; i < rowNumNonZeros; i++)
              oneRow[rowColIdxs[i]] = rowValues[i];

          for(size_t c = 0; c < numCols; c++) {
              printValue(os, oneRow[c]);
              if (c < numCols - 1)
                  os << ' ';
          }
          os << std::endl;
      }
      delete[] oneRow;
  }

  MCSRMatrix* sliceRow(size_t rowLowerIncl, size_t rowUpperExcl) const override {
    assert(rowUpperExcl<numRows && "Indices out of bounds");
    return DataObjectFactory::create<MCSRMatrix>(this, rowLowerIncl, rowUpperExcl);
      //throw std::runtime_error("TODO");
  }

  MCSRMatrix* sliceCol(size_t cl, size_t cu) const override {
      throw std::runtime_error("MCSRMatrix does not support sliceCol yet");
  }

  MCSRMatrix* slice(size_t rl, size_t ru, size_t cl, size_t cu) const override {
      throw std::runtime_error("MCSRMatrix does not support slice yet");
  }

  size_t serialize(std::vector<char> &buf) const override{
    // MCSRMatrix is not yet integrated into DaphneSerializer
    return 0;
  }


 };


 template <typename ValueType>
 std::ostream & operator<<(std::ostream & os, const MCSRMatrix<ValueType> & obj)
 {
     obj.print(os);
     return os;
 }
