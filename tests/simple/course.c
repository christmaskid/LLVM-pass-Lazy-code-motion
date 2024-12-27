#include <stdio.h>

// "load" and "store" 
// - with both pointers: are "id" instructions, not expressions
// - with immediates: are expressions

int main()
{ 
  // 0
  int r0, r1, r2, r3, r4, r5, r6, r7, r8, r17, r18, r19, r20, r21;

  int m[3] = {1,2,3}; // store i32 0, ptr %1, align 4
                      // %16 = alloca [3 x i32], align 4
                      // call void @llvm.memcpy.p0.p0.i64(ptr align 4 %16, ptr align 4 @__const.main.m, i64 12, i1 false)
  r17 = 17;       // store i32 17, ptr %11, align 4
  r18 = 18;       // store i32 18, ptr %12, align 4
  r19 = 19;       // store i32 19, ptr %13, align 4
  r20 = 20;       // store i32 20, ptr %14, align 4

  r0 = 0;         // store i32 0, ptr %2, align 4
  r1 = 1;         // store i32 1, ptr %3, align 4
  r2 = r1;        
                  // %17 = load i32, ptr %3, align 4
                  // store i32 %17, ptr %4, align 4  
  r3 = m[r0];     
                  // %18 = load i32, ptr %2, align 4
                  // %19 = sext i32 %18 to i64
                  // %20 = getelementptr inbounds [3 x i32], ptr %16, i64 0, i64 %19
                  // %21 = load i32, ptr %20, align 4
                  // store i32 %21, ptr %5, align 4
  r4 = r3;        
                  // %22 = load i32, ptr %5, align 4
                  // store i32 %22, ptr %6, align 4
  r5 = (r2 < r4); 
                  // %23 = load i32, ptr %4, align 4
                  // %24 = load i32, ptr %6, align 4
                  // %25 = icmp slt i32 %23, %24
                  // %26 = zext i1 %25 to i32
                  // store i32 %26, ptr %7, align 4
                  // %27 = load i32, ptr %7, align 4
                  // %28 = icmp ne i32 %27, 0
  if (r5) {       // br i1 %28, label %29, label %49
  // 29:                                               ; preds = %0
    do {          // br label %30
  // 30:                                               ; preds = %45, %29
      r20 = r17 * r18;  
                        // %31 = load i32, ptr %11, align 4
                        // %32 = load i32, ptr %12, align 4
                        // %33 = mul nsw i32 %31, %32
                        // store i32 %33, ptr %14, align 4
      r21 = r19 + r20;  
                        // %34 = load i32, ptr %13, align 4
                        // %35 = load i32, ptr %14, align 4
                        // %36 = add nsw i32 %34, %35
                        // store i32 %36, ptr %15, align 4
      r8 = r21;
                        // %37 = load i32, ptr %15, align 4
                        // store i32 %37, ptr %10, align 4
      r6 = r2 + 1;
                        // %38 = load i32, ptr %4, align 4
                        // %39 = add nsw i32 %38, 1
                        // store i32 %39, ptr %8, align 4
      r2 = r6;
                        // %40 = load i32, ptr %8, align 4
                        // store i32 %40, ptr %4, align 4
      r7 = (r2 > r4);
                        // %41 = load i32, ptr %4, align 4
                        // %42 = load i32, ptr %6, align 4
                        // %43 = icmp sgt i32 %41, %42
                        // %44 = zext i1 %43 to i32
                        // store i32 %44, ptr %9, align 4

                        // br label %45
      printf("%d\n", r7);
  // 45:                                               ; preds = %30
    } while(r7);        
                        // %46 = load i32, ptr %9, align 4
                        // %47 = icmp ne i32 %46, 0
                        // br i1 %47, label %30, label %48, !llvm.loop !6
  // 48:                                               ; preds = %45
                        // br label %49
  }
  // 49:                                               ; preds = %48, %0
  return 0;              //   ret i32 0
}