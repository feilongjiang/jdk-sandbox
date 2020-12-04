/*
 * Copyright (c) 2020, 2021, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package java.nio;

import java.lang.ref.Reference;
import java.lang.reflect.Array;
import java.util.Objects;
import jdk.internal.access.foreign.MemorySegmentProxy;

abstract class AbstractBufferImpl<B extends AbstractBufferImpl<B,A>, A> extends Buffer {

    final Object attachment;

    AbstractBufferImpl(long addr, Object hb,
                       int mark, int pos, int lim, int cap,
                       boolean readOnly,
                       Object attachment,
                       MemorySegmentProxy segment) {
        super(addr, hb, mark, pos, lim, cap, readOnly, segment);
        this.attachment = attachment;
    }

    /**
     * True if the buffer is big-endian. Otherwise, false for little-endian.
     */
    abstract boolean bigEndian();

    /**
     * A scale factor, expressed as a number of shift positions associated with
     * the element size, which is used to turn a logical buffer index into a
     * concrete address (usable from unsafe access). That is, for an int buffer
     * given that each buffer index covers 4 bytes, the buffer index needs to be
     * shifted by 2 in order to obtain the corresponding address offset. For
     * performance reasons, it is best to override this method in each subclass,
     * so that the scale factor becomes effectively a constant.
     */
    abstract int scaleFactor();

    /**
     * The base object for the unsafe access. Must be overridden by concrete
     * subclasses so that (i) cases where base == null are explicit in the code
     * and (ii) cases where base != null also feature a cast to the correct
     * array type. Unsafe access intrinsics can work optimally only if both
     * conditions are met.
     */
    abstract Object base();

    /**
     * The array carrier associated with this buffer; e.g. a ShortBuffer will
     * have a short[] carrier.
     */
    abstract Class<A> carrier();

    /**
     * Creates a new buffer of the same type as this buffer, with the given
     * properties. This method is used to implement various methods featuring
     * covariant override.
     */
    abstract B dup(int offset, int mark, int pos, int lim, int cap, boolean readOnly);

    @Override
    public B slice() {
        final int pos = this.position();
        final int lim = this.limit();
        final int rem = (pos <= lim ? lim - pos : 0);
        final int off = (pos << scaleFactor());
        return dup(off, -1, 0, rem, rem, readOnly);
    }

    @Override
    public B slice(int index, int length) {
        Objects.checkFromIndexSize(index, length, limit());
        final int off = (index << scaleFactor());
        return dup(off, -1, 0, length, length, readOnly);
    }

    @Override
    public B duplicate() {
        return dup(0, markValue(), position(), limit(), capacity(), readOnly);
    }

    public B asReadOnlyBuffer() {
        return dup(0, markValue(), position(), limit(), capacity(), true);
    }

    // access primitives

    /** Returns the address/offset for the given buffer position. */
    final long ix(int pos) {
        return address + ((long)pos << scaleFactor());
    }

    final byte getByteImpl(int pos) {
        try {
            return SCOPED_MEMORY_ACCESS.getByte(scope(), hb, ix(pos));
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putByteImpl(int pos, byte value) {
        try {
            SCOPED_MEMORY_ACCESS.putByte(scope(), base(), ix(pos), value);
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final char getCharImpl(int pos) {
        try {
            return SCOPED_MEMORY_ACCESS.getCharUnaligned(scope(), hb, ix(pos), bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putCharImpl(int pos, char value) {
        try {
            SCOPED_MEMORY_ACCESS.putCharUnaligned(scope(), hb, ix(pos), value, bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final short getShortImpl(int pos) {
        try {
            return SCOPED_MEMORY_ACCESS.getShortUnaligned(scope(), hb, ix(pos), bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putShortImpl(int pos, short value) {
        try {
            SCOPED_MEMORY_ACCESS.putShortUnaligned(scope(), hb, ix(pos), value, bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final int getIntImpl(int pos) {
        try {
            return SCOPED_MEMORY_ACCESS.getIntUnaligned(scope(), hb, ix(pos), bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putIntImpl(int pos, int value) {
        try {
            SCOPED_MEMORY_ACCESS.putIntUnaligned(scope(), hb, ix(pos), value, bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final long getLongImpl(int pos) {
        try {
            return SCOPED_MEMORY_ACCESS.getLongUnaligned(scope(), hb, ix(pos), bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putLongImpl(int pos, long value) {
        try {
            SCOPED_MEMORY_ACCESS.putLongUnaligned(scope(), hb, ix(pos), value, bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final float getFloatImpl(int pos) {
        try {
            int x = SCOPED_MEMORY_ACCESS.getIntUnaligned(scope(), hb, ix(pos), bigEndian());
            return Float.intBitsToFloat(x);
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putFloatImpl(int pos, float value) {
        try {
            int x = Float.floatToRawIntBits(value);
            SCOPED_MEMORY_ACCESS.putIntUnaligned(scope(), hb, ix(pos), x, bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final double getDoubleImpl(int pos) {
        try {
            long x = SCOPED_MEMORY_ACCESS.getLongUnaligned(scope(), hb, ix(pos), bigEndian());
            return Double.longBitsToDouble(x);
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putDoubleImpl(int pos, double value) {
        try {
            long x = Double.doubleToRawLongBits(value);
            SCOPED_MEMORY_ACCESS.putLongUnaligned(scope(), hb, ix(pos), x, bigEndian());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    // bulk access primitives

    final void getBulkImpl(A dst, int offset, int length) {
        Objects.checkFromIndexSize(offset, length, Array.getLength(dst));
        final int pos = position();
        final int lim = limit();
        assert (pos <= lim);  // TODO: remove
        final int rem = (pos <= lim ? lim - pos : 0);
        if (length > rem)
            throw new BufferUnderflowException();
        getBulkInternal(pos, dst, offset, length);
        position(pos + length);
    }

    final void getBulkImpl(int index, A dst, int offset, int length) {
        Objects.checkFromIndexSize(index, length, limit());
        Objects.checkFromIndexSize(offset, length, Array.getLength(dst));
        getBulkInternal(index, dst, offset, length);
    }

    private final void getBulkInternal(int index, A dst, int offset, int length) {
        long dstOffset = UNSAFE.arrayBaseOffset(carrier()) +
                ((long)offset << scaleFactor());
        try {
            if (scaleFactor() > 0 && bigEndian() != NORD_IS_BIG)
                SCOPED_MEMORY_ACCESS.copySwapMemory(scope(), null,
                        base(), ix(index),
                        dst, dstOffset,
                        (long)length << scaleFactor(),
                        (long)1 << scaleFactor());
            else
                SCOPED_MEMORY_ACCESS.copyMemory(scope(), null,
                        base(), ix(index),
                        dst, dstOffset,
                        (long)length << scaleFactor());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putBulkImpl(A src, int offset, int length) {
        Objects.checkFromIndexSize(offset, length, Array.getLength(src));
        if (readOnly)
            throw new ReadOnlyBufferException();
        final int pos = position();
        final int lim = limit();
        assert (pos <= lim);
        final int rem = (pos <= lim ? lim - pos : 0);
        if (length > rem)
            throw new BufferOverflowException();
        putBulkInternal(pos, src, offset, length);
        position(pos + length);
    }

    final void putBulkImpl(int index, A src, int offset, int length) {
        Objects.checkFromIndexSize(index, length, limit());
        Objects.checkFromIndexSize(offset, length, Array.getLength(src));
        if (readOnly)
            throw new ReadOnlyBufferException();
        putBulkInternal(index, src, offset, length);
    }

    private final void putBulkInternal(int index, A src, int offset, int length) {
        long srcOffset = UNSAFE.arrayBaseOffset(carrier()) +
                ((long)offset << scaleFactor());
        try {
            if (scaleFactor() > 0 && bigEndian() != NORD_IS_BIG)
                SCOPED_MEMORY_ACCESS.copySwapMemory(null, scope(),
                        src, srcOffset,
                        base(), ix(index),
                        (long)length << scaleFactor(),
                        (long)1 << scaleFactor());
            else
                SCOPED_MEMORY_ACCESS.copyMemory(null, scope(),
                        src, srcOffset,
                        base(), ix(index),
                        (long)length << scaleFactor());
        } finally {
            Reference.reachabilityFence(this);
        }
    }

    final void putBulkImpl(B src) {
        if (src == this)
            throw createSameBufferException();
        if (readOnly)
            throw new ReadOnlyBufferException();
        final int srcPos = src.position();
        final int srcLim= src.limit();
        final int dstPos = position();
        final int dstLim = limit();
        final int srcLength    = (srcPos <= srcLim ? srcLim - srcPos : 0);
        final int dstRemaining = (dstPos <= dstLim ? dstLim - dstPos : 0);
        if (srcLength > dstRemaining)
            throw new BufferOverflowException();
        putBulkInternal(dstPos, src, srcPos, srcLength);
        position(dstPos + srcLength);
        src.position(srcPos + srcLength);
    }

    final void putBulkImpl(int index, B src, int offset, int length) {
        Objects.checkFromIndexSize(index, length, limit());
        Objects.checkFromIndexSize(offset, length, src.limit());
        if (readOnly)
            throw new ReadOnlyBufferException();
        putBulkInternal(index, src, offset, length);
    }

    private final void putBulkInternal(int index, B src, int srcPos, int length) {
        try {
            if (scaleFactor() > 0 && bigEndian() != src.bigEndian())
                SCOPED_MEMORY_ACCESS.copySwapMemory(src.scope(), scope(),
                        src.base(), src.ix(srcPos),
                        base(), ix(index),
                        (long) length << scaleFactor(),
                        (long) 1 << scaleFactor());
            else
                SCOPED_MEMORY_ACCESS.copyMemory(src.scope(), scope(),
                        src.base(), src.ix(srcPos),
                        base(), ix(index),
                        (long) length << scaleFactor());
        } finally {
            Reference.reachabilityFence(src);
            Reference.reachabilityFence(this);
        }
    }

    // ---

    public boolean hasArray() {
        return hb != null && hb.getClass() == carrier() && !readOnly;
    }

    public A array() {
        if (hb == null || hb.getClass() != carrier())
            throw new UnsupportedOperationException();
        if (readOnly)
            throw new ReadOnlyBufferException();
        @SuppressWarnings("unchecked") A a = (A)hb;
        return a;
    }

    public int arrayOffset() {
        if (hb == null || hb.getClass() != carrier())
            throw new UnsupportedOperationException();
        if (readOnly)
            throw new ReadOnlyBufferException();
        assert (address & 0xFFFFFFFF00000000L) == 0;
        return ((int)address - UNSAFE.arrayBaseOffset(carrier())) >> scaleFactor();
    }

    public B compact() {
        if (readOnly)
            throw new ReadOnlyBufferException();
        int pos = position();
        int lim = limit();
        int rem = (pos <= lim ? lim - pos : 0);
        try {
            SCOPED_MEMORY_ACCESS.copyMemory(scope(), scope(), base(), ix(pos), base(), ix(0), (long)rem << scaleFactor());
        } finally {
            Reference.reachabilityFence(this);
        }
        position(rem);
        limit(capacity());
        discardMark();
        @SuppressWarnings("unchecked") B b = (B)this;
        return b;
    }

    @Override
    public String toString() {
        StringBuffer sb = new StringBuffer();
        sb.append(getClass().getName());
        sb.append("[pos=");
        sb.append(position());
        sb.append(" lim=");
        sb.append(limit());
        sb.append(" cap=");
        sb.append(capacity());
        sb.append("]");
        return sb.toString();
    }

    // TODO equals / hashCode ??


    // direct buffer utils

    final Object attachmentValueOrThis() {
        return attachment != null ? attachment : this;
    }
}
