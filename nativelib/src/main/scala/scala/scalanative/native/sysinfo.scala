package scala.scalanative.native

import scala.scalanative.unsafe.{CInt, extern}

@extern
object sysinfo {
  def get_nprocs: CInt = extern
}
