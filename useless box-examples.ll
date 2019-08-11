define void @"scala.scalanative.runtime.Array$HeaderOps$::stride$underscore$=$extension_ptr_i64_unit"(i8* %_1, i8* %_2, i64 %_3) alwaysinline personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
_4.0:
  %_11 = bitcast i8* %_2 to { i8*, i32, i32 }*
  %_12 = getelementptr { i8*, i32, i32 }, { i8*, i32, i32 }* %_11, i32 0, i32 2
  %_5 = bitcast i32* %_12 to i8*
  %_6 = trunc i64 %_3 to i32
  %_7 = call i8* (i8*, i32) @"scala.runtime.BoxesRunTime$::boxToInteger_i32_java.lang.Integer"(i8* undef, i32 %_6)
  %_13 = bitcast i8* %_5 to i32*
  store i32 %_6, i32* %_13
  ret void
}

define void @"scala.scalanative.runtime.Array$HeaderOps$::length$underscore$=$extension_ptr_i32_unit"(i8* %_1, i8* %_2, i32 %_3) alwaysinline personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
_4.0:
  %_10 = bitcast i8* %_2 to { i8*, i32, i32 }*
  %_11 = getelementptr { i8*, i32, i32 }, { i8*, i32, i32 }* %_10, i32 0, i32 1
  %_5 = bitcast i32* %_11 to i8*
  %_6 = call i8* (i8*, i32) @"scala.runtime.BoxesRunTime$::boxToInteger_i32_java.lang.Integer"(i8* undef, i32 %_3)
  %_12 = bitcast i8* %_5 to i32*
  store i32 %_3, i32* %_12
  ret void
}
