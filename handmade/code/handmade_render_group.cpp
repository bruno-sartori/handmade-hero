inline v4 Unpack4x8(uint32 Packed) {
  v4 Result = {
    (real32)((Packed >> 16) & 0xFF),
    (real32)((Packed >> 8) & 0xFF),
    (real32)((Packed >> 0) & 0xFF),
    (real32)((Packed >> 24) & 0xFF)
  };

  return Result;
}

inline v4 SRGB255ToLinear1(v4 C) {
  v4 Result;

  real32 Inv255 = 1.0f / 255.0f;

  Result.r = Square(Inv255 * C.r);
  Result.g = Square(Inv255 * C.g);
  Result.b = Square(Inv255 * C.b);
  Result.a = Inv255 * C.a;

  return Result;
}

inline v4 Linear1ToSRGB255(v4 C) {
  v4 Result;

  real32 One255 = 255.0f;

  Result.r = One255 * SquareRoot(C.r);
  Result.g = One255 * SquareRoot(C.g);
  Result.b = One255 * SquareRoot(C.b);
  Result.a = 255.0f * C.a;

  return Result;
}

inline v4 UnscaleAndBiasNormal(v4 Normal) {
  v4 Result;
  real32 Inv255 = 1.0f / 255.0f;

  Result.x = -1.0f + 2.0f * (Inv255 * Normal.x);
  Result.y = -1.0f + 2.0f * (Inv255 * Normal.y);
  Result.z = -1.0f + 2.0f * (Inv255 * Normal.z);
  Result.w = Inv255 * Normal.w;

  return Result;
}

internal void DrawRectangle(loaded_bitmap *Buffer, v2 vMin, v2 vMax, v4 Color) {
  real32 R = Color.r;
  real32 G = Color.g;
  real32 B = Color.b;
  real32 A = Color.a;

  int32 MinX = RoundReal32ToInt32(vMin.x);
  int32 MinY = RoundReal32ToInt32(vMin.y);
  int32 MaxX = RoundReal32ToInt32(vMax.x);
  int32 MaxY = RoundReal32ToInt32(vMax.y);

  if (MinX < 0) {
    MinX = 0;
  }

  if (MinY < 0) {
    MinY = 0;
  }

  if (MaxX > Buffer->Width) {
    MaxX = Buffer->Width;
  }

  if (MaxY > Buffer->Height) {
    MaxY = Buffer->Height;
  }

  uint32 Color32 = ((RoundReal32ToUInt32(A * 255.0f) << 24) | (RoundReal32ToUInt32(R * 255.0f) << 16) | (RoundReal32ToUInt32(G * 255.0f) << 8) | (RoundReal32ToUInt32(B * 255.0f) << 0));
  uint8 *Row = ((uint8 *)Buffer->Memory + MinX * BITMAP_BITES_PER_PIXEL + MinY * Buffer->Pitch);

  for (int Y = MinY; Y < MaxY; ++Y) {
    uint32 *Pixel = (uint32 *)Row;
    for (int X = MinX; X < MaxX; ++X) {
      *Pixel++ = Color32;
    }
    Row += Buffer->Pitch;
  }
}

struct bilinear_sample {
  uint32 A, B, C, D;
};

inline bilinear_sample BilinearSample(loaded_bitmap *Texture, int32 X, int32 Y) {
  bilinear_sample Result;

  uint8 *TexelPtr = ((uint8 *)Texture->Memory) + Y * Texture->Pitch + X * sizeof(uint32);
  Result.A = *(uint32 *)(TexelPtr);
  Result.B = *(uint32 *)(TexelPtr + sizeof(uint32));
  Result.C = *(uint32 *)(TexelPtr + Texture->Pitch);
  Result.D = *(uint32 *)(TexelPtr + Texture->Pitch + sizeof(uint32));

  return Result;
}

inline v4 SRGBBilinearBlend(bilinear_sample TexelSample, real32 fX, real32 fY) {
  v4 TexelA = Unpack4x8(TexelSample.A);
  v4 TexelB = Unpack4x8(TexelSample.B);
  v4 TexelC = Unpack4x8(TexelSample.C);
  v4 TexelD = Unpack4x8(TexelSample.D);

  // NOTE: Go from sRGB to "linear" brightness space
  TexelA = SRGB255ToLinear1(TexelA);
  TexelB = SRGB255ToLinear1(TexelB);
  TexelC = SRGB255ToLinear1(TexelC);
  TexelD = SRGB255ToLinear1(TexelD);

  v4 Result = Lerp(Lerp(TexelA, fX, TexelB), fY, Lerp(TexelC, fX, TexelD));

  return Result;
}

inline v3 SampleEnvironmentMap(v2 ScreenSpaceUV, v3 SampleDirection, real32 Roughness, environment_map *Map, real32 DistanceFromMapInZ) {
  /* NOTE:

    ScreenSpaceUV tells us where the ray is being cast
    FROM in normalized screen coordinates.

    SampleDirection tells us what direction the cast is going -
    it does not have to be normalized.

    Roughness says which LODs of Map we sample from.

    DistanceFromMapInZ says how far the map is from the sample point in Z, given
    in meters.
  */

  // NOTE: Pick which LOD to sample from
  uint32 LODIndex = (uint32)(Roughness * (real32)(ArrayCount(Map->LOD) - 1) + 0.5f);
  Assert(LODIndex < ArrayCount(Map->LOD));

  loaded_bitmap *LOD = &Map->LOD[LODIndex];

  // NOTE: Compute the distance to the map and the scaling
  // factoir for meters-to-UVs
  real32 UVsPerMeter = 0.1f; //TODO: Parameterize this, and should be differente for X and Y based on map!
  real32 C = (UVsPerMeter * DistanceFromMapInZ) / SampleDirection.y;
  v2 Offset = C * V2(SampleDirection.x, SampleDirection.z);

  // NOTE: Find the intersection point
  v2 UV = ScreenSpaceUV + Offset;

  // NOTE: Clamp to the valid range
  UV.x = Clamp01(UV.x);
  UV.y = Clamp01(UV.y);

  // NOTE: Bilinear sample
  // TODO: Formalize texture boundaries!!!
  real32 tX = ((UV.x * (real32)(LOD->Width - 2)));
  real32 tY = ((UV.y * (real32)(LOD->Height - 2)));

  int32 X = (int32)tX;
  int32 Y = (int32)tY;

  real32 fX = tX - (real32)X;
  real32 fY = tY - (real32)Y;

  Assert((X >= 0) && (X < LOD->Width));
  Assert((Y >= 0) && (Y < LOD->Height));

#if 0
  // NOTE: Turn this on to see where in the map you're sampling
  uint8 *TexelPtr = ((uint8 *)LOD->Memory) + Y * LOD->Pitch + X * sizeof(uint32);
  *(uint32 *)TexelPtr = 0xFFFFFFFF;
#endif

  bilinear_sample Sample = BilinearSample(LOD, X, Y);
  v3 Result = SRGBBilinearBlend(Sample, fX, fY).xyz;

  return Result;
}

internal void DrawRectangleSlowly(loaded_bitmap *Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color, loaded_bitmap *Texture, loaded_bitmap *NormalMap, environment_map *Top, environment_map *Middle, environment_map *Bottom, real32 PixelsToMeters) {
  // NOTE: Premultiply color up front
  Color.rgb *= Color.a;

  real32 XAxisLength = Length(XAxis);
  real32 YAxisLength = Length(YAxis);
  v2 NxAxis = (YAxisLength / XAxisLength) * XAxis;
  v2 NyAxis = (XAxisLength / YAxisLength) * YAxis;

  // NOTE: NzScale could be a parameter if we want people to
  // have control over the amount of scaling in the Z direction
  // that the normals appear to have
  real32 NzScale = 0.5f * (XAxisLength + YAxisLength);

  real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
  real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

  uint32 Color32 = (
    (RoundReal32ToUInt32(Color.a * 255.0f) << 24) |
    (RoundReal32ToUInt32(Color.r * 255.0f) << 16) |
    (RoundReal32ToUInt32(Color.g * 255.0f) << 8) |
    (RoundReal32ToUInt32(Color.b * 255.0f) << 0)
  );

  int32 WidthMax = (Buffer->Width -1);
  int32 HeightMax = (Buffer->Height -1);

  real32 InvWidthMax = 1.0f / (real32)WidthMax;
  real32 InvHeightMax = 1.0f / (real32)HeightMax;

    // TODO: This will need to be specified separately!!!
  real32 OriginZ = 0.0f;
  real32 OriginY = (Origin + 0.5f * XAxis + 0.5f * YAxis).y;
  real32 FixedCastY = InvHeightMax * OriginY;

  int32 XMin = WidthMax;
  int32 XMax = 0;
  int32 YMin = HeightMax;
  int32 YMax = 0;

  v2 P[4] = { Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis };
  for (uint32 PIndex = 0; PIndex < ArrayCount(P); ++PIndex) {
    v2 TestP = P[PIndex];
    int32 FloorX = FloorReal32ToInt32(TestP.x);
    int32 CeilX = CeilReal32ToInt32(TestP.x);
    int32 FloorY = FloorReal32ToInt32(TestP.y);
    int32 CeilY = CeilReal32ToInt32(TestP.y);

    if (XMin > FloorX) { XMin = FloorX; }
    if (YMin > FloorY) { YMin = FloorY; }
    if (XMax < CeilX) { XMax = CeilX; }
    if (YMax < CeilY) { YMax = CeilY; }
  }

  if (XMin < 0) { XMin = 0; }
  if (YMin < 0) { YMin = 0; }
  if (XMax > WidthMax) { XMax = WidthMax; }
  if (YMax > HeightMax) { YMax = HeightMax; }

  uint8 *Row = ((uint8 *)Buffer->Memory + XMin * BITMAP_BITES_PER_PIXEL + YMin * Buffer->Pitch);
  for (int Y = YMin; Y <= YMax; ++Y) {
    uint32 *Pixel = (uint32 *)Row;

    for (int X = XMin; X <= XMax; ++X) {
      v2 PixelP = V2i(X, Y);
      v2 d = PixelP - Origin;

      // NOTE: See tutorial 92 for explanation
      // TODO: PerpInner
      // TODO: Simpler Origin
      real32 Edge0 = Inner(d, -Perp(XAxis));
      real32 Edge1 = Inner(d - XAxis, -Perp(YAxis));
      real32 Edge2 = Inner(d - XAxis - YAxis, Perp(XAxis));
      real32 Edge3 = Inner(d - YAxis, Perp(YAxis));

      if ((Edge0 < 0) && (Edge1 < 0) && (Edge2 < 0) && (Edge3 < 0)) {
#if 1
        v2 ScreenSpaceUV = { InvWidthMax * (real32)X, FixedCastY };
        real32 ZDiff = PixelsToMeters * ((real32)Y - OriginY);
#else
        v2 ScreenSpaceUV = { InvWidthMax * (real32)X, InvHeightMax * (real32)Y };
        real32 ZDiff = 0;
#endif

        real32 U = InvXAxisLengthSq * Inner(d, XAxis);
        real32 V = InvYAxisLengthSq * Inner(d, YAxis);

#if 0
        // TODO: SSE Clamping
        Assert((U >= 0.0f) && (U <= 1.0f));
        Assert((V >= 0.0f) && (V <= 1.0f));
#endif
        // TODO: Formalize texture boundries!!
        real32 tX = ((U * (real32)(Texture->Width - 2)));
        real32 tY = ((V * (real32)(Texture->Height - 2)));

        int32 X = (int32)tX;
        int32 Y = (int32)tY;

        real32 fX = tX - (real32)X;
        real32 fY = tY - (real32)Y;

        Assert((X >= 0) && (X < Texture->Width));
        Assert((Y >= 0) && (Y < Texture->Height));

        bilinear_sample TexelSample = BilinearSample(Texture, X, Y);

        v4 Texel = SRGBBilinearBlend(TexelSample, fX, fY);

        if (NormalMap) {
          bilinear_sample NormalSample = BilinearSample(NormalMap, X, Y);

          v4 NormalA = Unpack4x8(NormalSample.A);
          v4 NormalB = Unpack4x8(NormalSample.B);
          v4 NormalC = Unpack4x8(NormalSample.C);
          v4 NormalD = Unpack4x8(NormalSample.D);

          v4 Normal = Lerp(Lerp(NormalA, fX, NormalB), fY, Lerp(NormalC, fX, NormalD));
          Normal = UnscaleAndBiasNormal(Normal);
          // TODO: Do we really need to do this?

          Normal.xy = Normal.x * NxAxis + Normal.y * NyAxis; // Rotate normals based on X/Y axis!
          Normal.z *= NzScale;
          Normal.xyz = Normalize(Normal.xyz);


          // NOTE: The eye vector is always assumed to be [0, 0, 1]
          // This is just the simplified version of the reflection e + 2e^T N N
          v3 BounceDirection = 2.0f * Normal.z * Normal.xyz;
          BounceDirection.z -= 1.0f;
          // TODO: Eventually we need to support two mappings,
          // one for topdown view (which we dont do now) and one
          // for sideways, which is whats happening here.
          BounceDirection.z = -BounceDirection.z;

          environment_map *FarMap = 0;
          real32 Pz = OriginZ + ZDiff;
          real32 MapZ = 2.0f;
          real32 tEnvMap = BounceDirection.y;
          real32 tFarMap = 0.0f;
          if (tEnvMap < -0.5f) {
            // TODO: This path seems PARTICULARLY broken!
            FarMap = Bottom;
            tFarMap = -1.0f - 2.0f * tEnvMap;
          } else if (tEnvMap > 0.5f) {
            FarMap = Top;
            tFarMap = 2.0f * (tEnvMap - 0.5f);
          }

          tFarMap *= tFarMap;
          tFarMap *= tFarMap;

          v3 LightColor = { 0, 0, 0 }; // TODO: How tdo we samplpe from the middle map??
          if (FarMap) {
            real32 DistanceFromMapInZ = FarMap->Pz - Pz;
            v3 FarMapColor = SampleEnvironmentMap(ScreenSpaceUV, BounceDirection, Normal.w, FarMap, DistanceFromMapInZ);
            LightColor = Lerp(LightColor, tFarMap, FarMapColor);
          }

          // TODO: ? Actually do a lighting model computation here
          Texel.rgb = Texel.rgb + Texel.a * LightColor;
#if 0
          // NOTE: Draws the bounce direction
          Texel.rgb = V3(0.5f, 0.5f, 0.5f) + 0.5f * BounceDirection;
          Texel.rgb *= Texel.a;
#endif
        }

        Texel = Hadamard(Texel, Color);
        Texel.r = Clamp01(Texel.r);
        Texel.g = Clamp01(Texel.g);
        Texel.b = Clamp01(Texel.b);

        v4 Dest = {
          (real32)((*Pixel >> 16) & 0xFF),
          (real32)((*Pixel >> 8) & 0xFF),
          (real32)((*Pixel >> 0) & 0xFF),
          (real32)((*Pixel >> 24) & 0xFF),
        };

        // NOTE: Go from sRGB to "linear" brightness space
        Dest = SRGB255ToLinear1(Dest);

        v4 Blended = (1.0f - Texel.a) * Dest + Texel;

        // NOTE: Go back from "linear" brightness space to sRGB
        v4 Blended255 = Linear1ToSRGB255(Blended);

        *Pixel = (
          ((uint32)(Blended255.a + 0.5f) << 24) |
          ((uint32)(Blended255.r + 0.5f) << 16) |
          ((uint32)(Blended255.g + 0.5f) << 8) |
          ((uint32)(Blended255.b + 0.5f) << 0)
        );
      }

      ++Pixel;
    }
    Row += Buffer->Pitch;
  }
}

internal void DrawBitmap(loaded_bitmap *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY, real32 CAlpha = 1.0f) {
  int32 MinX = RoundReal32ToInt32(RealX);
  int32 MinY = RoundReal32ToInt32(RealY);
  int32 MaxX = MinX + Bitmap->Width;
  int32 MaxY = MinY + Bitmap->Height;

  int32 SourceOffsetX = 0;
  if (MinX < 0) {
    SourceOffsetX = -MinX;
    MinX = 0;
  }

  int32 SourceOffsetY = 0;
  if (MinY < 0) {
    SourceOffsetY = -MinY;
    MinY = 0;
  }

  if (MaxX > Buffer->Width) {
    MaxX = Buffer->Width;
  }

  if (MaxY > Buffer->Height) {
    MaxY = Buffer->Height;
  }

  uint8 *SourceRow = (uint8 *)Bitmap->Memory + SourceOffsetY * Bitmap->Pitch + BITMAP_BITES_PER_PIXEL * SourceOffsetX;
  uint8 *DestRow = ((uint8 *)Buffer->Memory + MinX * BITMAP_BITES_PER_PIXEL + MinY * Buffer->Pitch);

  for (int Y = MinY; Y < MaxY; ++Y) {
    uint32 *Dest = (uint32 *)DestRow;
    uint32 *Source = (uint32 *)SourceRow;

    for (int X = MinX; X < MaxX; ++X) {
      v4 Texel = {
        CAlpha * (real32)((*Source >> 16) & 0xFF),
        CAlpha * (real32)((*Source >> 8) & 0xFF),
        CAlpha * (real32)((*Source >> 0) & 0xFF),
        (real32)((*Source >> 24) & 0xFF)
      };
      Texel = SRGB255ToLinear1(Texel);

      Texel *= CAlpha;

      v4 D = {
        (real32)((*Dest >> 16) & 0xFF),
        (real32)((*Dest >> 8) & 0xFF),
        (real32)((*Dest >> 0) & 0xFF),
        (real32)((*Dest >> 24) & 0xFF)
      };
      D = SRGB255ToLinear1(D);

      v4 Result = (1.0f - Texel.a) * D + Texel;

      Result = Linear1ToSRGB255(Result);

      *Dest = (
        ((uint32)(Result.a + 0.5f) << 24) |
        ((uint32)(Result.r + 0.5f) << 16) |
        ((uint32)(Result.g + 0.5f) << 8) |
        ((uint32)(Result.b + 0.5f) << 0)
      );

      ++Dest;
      ++Source;
    }

    DestRow += Buffer->Pitch;
    SourceRow += Bitmap->Pitch;
  }
}

internal void ChangeSaturation(loaded_bitmap *Buffer, real32 Level) {
  uint8 *DestRow = (uint8 *)Buffer->Memory;

  for (int Y = 0; Y < Buffer->Height; ++Y) {
    uint32 *Dest = (uint32 *)DestRow;

    for (int X = 0; X < Buffer->Width; ++X) {
      v4 D = {
        (real32)((*Dest >> 16) & 0xFF),
        (real32)((*Dest >> 8) & 0xFF),
        (real32)((*Dest >> 0) & 0xFF),
        (real32)((*Dest >> 24) & 0xFF)
      };
      D = SRGB255ToLinear1(D);

      real32 Avg = (1.0f / 3.0f) * (D.r + D.g + D.b);
      v3 Delta = V3(D.r - Avg, D.g - Avg, D.b - Avg);

      v4 Result = V4(V3(Avg, Avg, Avg) + Level * Delta, D.a);

      Result = Linear1ToSRGB255(Result);

      *Dest = (
        ((uint32)(Result.a + 0.5f) << 24) |
        ((uint32)(Result.r + 0.5f) << 16) |
        ((uint32)(Result.g + 0.5f) << 8) |
        ((uint32)(Result.b + 0.5f) << 0)
      );

      ++Dest;
    }

    DestRow += Buffer->Pitch;
  }
}

inline void DrawRectangleOutline(loaded_bitmap *Buffer, v2 vMin, v2 vMax, v3 Color, real32 R = 2.0f) {
  // NOTE: Top and bottom
  DrawRectangle(Buffer, V2(vMin.x - R, vMin.y - R), V2(vMax.x + R, vMin.y + R), V4(Color, 1.0f));
  DrawRectangle(Buffer, V2(vMin.x - R, vMax.y - R), V2(vMax.x + R, vMax.y + R), V4(Color, 1.0f));

  // NOTE: Left and right
  DrawRectangle(Buffer, V2(vMin.x - R, vMin.y - R), V2(vMin.x + R, vMax.y + R), V4(Color, 1.0f));
  DrawRectangle(Buffer, V2(vMax.x - R, vMin.y - R), V2(vMax.x + R, vMax.y + R), V4(Color, 1.0f));
}

internal void DrawMatte(loaded_bitmap *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY, real32 CAlpha = 1.0f) {
  int32 MinX = RoundReal32ToInt32(RealX);
  int32 MinY = RoundReal32ToInt32(RealY);
  int32 MaxX = MinX + Bitmap->Width;
  int32 MaxY = MinY + Bitmap->Height;

  int32 SourceOffsetX = 0;
  if (MinX < 0) {
    SourceOffsetX = -MinX;
    MinX = 0;
  }

  int32 SourceOffsetY = 0;
  if (MinY < 0) {
    SourceOffsetY = -MinY;
    MinY = 0;
  }

  if (MaxX > Buffer->Width) {
    MaxX = Buffer->Width;
  }

  if (MaxY > Buffer->Height) {
    MaxY = Buffer->Height;
  }

  uint8 *SourceRow = (uint8 *)Bitmap->Memory + SourceOffsetY * Bitmap->Pitch + BITMAP_BITES_PER_PIXEL * SourceOffsetX;
  uint8 *DestRow = ((uint8 *)Buffer->Memory + MinX * BITMAP_BITES_PER_PIXEL + MinY * Buffer->Pitch);

  for (int Y = MinY; Y < MaxY; ++Y) {
    uint32 *Dest = (uint32 *)DestRow;
    uint32 *Source = (uint32 *)SourceRow;

    for (int X = MinX; X < MaxX; ++X) {

      real32 SA = (real32)((*Source >> 24) & 0xFF);
      real32 RSA = (SA  / 255.0f) * CAlpha;
      real32 SR = CAlpha * (real32)((*Source >> 16) & 0xFF);
      real32 SG = CAlpha * (real32)((*Source >> 8) & 0xFF);
      real32 SB = CAlpha * (real32)((*Source >> 0) & 0xFF);

      real32 DA = (real32)((*Dest >> 24) & 0xFF);
      real32 DR = (real32)((*Dest >> 16) & 0xFF);
      real32 DG = (real32)((*Dest >> 8) & 0xFF);
      real32 DB = (real32)((*Dest >> 0) & 0xFF);
      real32 RDA = (DA / 255.0f);

      real32 InvRSA = (1.0f - RSA);
      // TODO: Check this for math errors
      //real32 A = 255.0f * (RSA + RDA - RSA*RDA);
      real32 A = InvRSA*DA;
      real32 R = InvRSA*DR;
      real32 G = InvRSA*DG;
      real32 B = InvRSA*DB;

      *Dest = (((uint32)(A + 0.5f) << 24) | ((uint32)(R + 0.5f) << 16) | ((uint32)(G + 0.5f) << 8) | ((uint32)(B + 0.5f) << 0));

      ++Dest;
      ++Source;
    }

    DestRow += Buffer->Pitch;
    SourceRow += Bitmap->Pitch;
  }
}

struct entity_basis_p_result {
  v2 P;
  real32 Scale;
};
inline entity_basis_p_result GetRenderEntityBasisP(render_group *RenderGroup, render_entity_basis *EntityBasis, v2 ScreenCenter) {
  entity_basis_p_result Result;
  // TODO: Figure out exactly how z-based XY displacement should work.
  v3 EntityBaseP = RenderGroup->MetersToPixels * EntityBasis->Basis->P;
  real32 ZFudge = 1.0f + 0.0015f * EntityBaseP.z; // scaling
  v2 EntityGroundPoint = ScreenCenter + ZFudge * (EntityBaseP.xy + EntityBasis->Offset.xy);
  v2 Center = EntityGroundPoint; // + V2(0, EntityBaseP.z + EntityBasis->Offset.z);

  Result.P = Center;
  Result.Scale = ZFudge;

  return Result;
}

internal void RenderGroupToOutput(render_group *RenderGroup, loaded_bitmap *OutputTarget) {
  v2 ScreenCenter = {
    0.5f * (real32)OutputTarget->Width,
    0.5f * (real32)OutputTarget->Height
  };

  real32 PixelsToMeters = 1.0f / RenderGroup->MetersToPixels;

  for(uint32 BaseAddress = 0; BaseAddress < RenderGroup->PushBufferSize;) {
    render_group_entry_header *Header = (render_group_entry_header *)(RenderGroup->PushBufferBase + BaseAddress);
    BaseAddress += sizeof(*Header);
    void *Data = (uint8 *)Header + sizeof(*Header);

    switch (Header->Type) {
      case RenderGroupEntryType_render_entry_clear: {
        render_entry_clear *Entry = (render_entry_clear *)Data;

        DrawRectangle(OutputTarget, V2(0.0f, 0.0f), V2((real32)OutputTarget->Width, (real32)OutputTarget->Height), Entry->Color);

        BaseAddress += sizeof(*Entry);
      } break;

      case RenderGroupEntryType_render_entry_bitmap: {
        render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
        entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenCenter);
        Assert(Entry->Bitmap);
#if 0
        DrawBitmap(OutputTarget, Entry->Bitmap, P.x, P.y, Entry->Color.a);
#else
        DrawRectangleSlowly(OutputTarget, Basis.P, Basis.Scale * V2i(Entry->Bitmap->Width, 0), Basis.Scale * V2i(0, Entry->Bitmap->Height), Entry->Color, Entry->Bitmap, 0, 0, 0, 0, PixelsToMeters);
#endif
        BaseAddress += sizeof(*Entry);
      } break;

      case RenderGroupEntryType_render_entry_rectangle: {
        render_entry_rectangle *Entry = (render_entry_rectangle *)Data;

        entity_basis_p_result Basis = GetRenderEntityBasisP(RenderGroup, &Entry->EntityBasis, ScreenCenter);
        DrawRectangle(OutputTarget, Basis.P, Basis.P + Basis.Scale * Entry->Dim, Entry->Color);

        BaseAddress += sizeof(*Entry);
      } break;

      case RenderGroupEntryType_render_entry_coordinate_system: {
        render_entry_coordinate_system *Entry = (render_entry_coordinate_system *)Data;

        v2 vMax = (Entry->Origin + Entry->XAxis + Entry->YAxis);
        DrawRectangleSlowly(OutputTarget, Entry->Origin, Entry->XAxis, Entry->YAxis, Entry->Color, Entry->Texture, Entry->NormalMap, Entry->Top, Entry->Middle, Entry->Bottom, PixelsToMeters);

        v4 Color = { 1, 1, 0, 1};
        v2 Dim = {2, 2};
        v2 P = Entry->Origin;
        DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

        P = Entry->Origin + Entry->XAxis;
        DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

        P = Entry->Origin + Entry->YAxis;
        DrawRectangle(OutputTarget, P - Dim, P + Dim, Color);

        DrawRectangle(OutputTarget, vMax - Dim, vMax + Dim, Color);

#if 0
        for (uint32 PIndex = 0; PIndex < ArrayCount(Entry->Points); ++PIndex) {
          v2 P = Entry->Points[PIndex];
          P = Entry->Origin + P.x * Entry->XAxis + P.y*Entry->YAxis;
          DrawRectangle(OutputTarget, P - Dim, P + Dim, Entry->Color.r, Entry->Color.g, Entry->Color.b);

        }
#endif
        BaseAddress += sizeof(*Entry);
      } break;

      InvalidDefaultCase;
    }


  }
}

internal render_group *AllocateRenderGroup(memory_arena *Arena, uint32 MaxPushBufferSize, real32 MetersToPixels) {
  render_group *Result = PushStruct(Arena, render_group);
  Result->PushBufferBase = (uint8 *)PushSize(Arena, MaxPushBufferSize);
  Result->DefaultBasis = PushStruct(Arena, render_basis);
  Result->DefaultBasis->P = V3(0, 0, 0);
  Result->MetersToPixels = MetersToPixels;
  Result->MaxPushBufferSize = MaxPushBufferSize;
  Result->PushBufferSize = 0;
  Result->GlobalAlpha = 1.0f;

  return Result;
}

#define PushRenderElement(Group, type) (type *)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type)
inline void *PushRenderElement_(render_group *Group, uint32 Size, render_group_entry_type Type) {
  void *Result = 0;

  Size += sizeof(render_group_entry_header);

  if (Group->PushBufferSize + Size < Group->MaxPushBufferSize) {
    render_group_entry_header *Header = (render_group_entry_header *)(Group->PushBufferBase + Group->PushBufferSize);
    Header->Type = Type;
    Result = (uint8 *)Header + sizeof(*Header);
    Group->PushBufferSize += Size;
  } else {
    InvalidCodePath;
  }

  return Result;
}

inline void PushBitmap(render_group *Group, loaded_bitmap *Bitmap, v3 Offset, v4 Color = V4(1, 1, 1, 1)) {
  render_entry_bitmap *Entry = PushRenderElement(Group, render_entry_bitmap);
  if (Entry) {
    Entry->EntityBasis.Basis = Group->DefaultBasis;
    Entry->Bitmap = Bitmap;
    Entry->EntityBasis.Offset = Group->MetersToPixels * Offset - V3(Bitmap->Align, 0);
    Entry->Color = Group->GlobalAlpha * Color;
  }
}

inline void PushRect(render_group *Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1)) {
  render_entry_rectangle *Entry = PushRenderElement(Group, render_entry_rectangle);
  if (Entry) {
    Entry->EntityBasis.Basis = Group->DefaultBasis;
    Entry->EntityBasis.Offset = Group->MetersToPixels * (Offset - V3(0.5f * Dim, 0));
    Entry->Color = Color;
    Entry->Dim = Group->MetersToPixels * Dim;
  }
}

inline void PushRectOutline(render_group *Group, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1)) {
  real32 Thickness = 0.1f;

  // NOTE: Top and bottom
  PushRect(Group, Offset - V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);
  PushRect(Group, Offset + V3(0, 0.5f * Dim.y, 0), V2(Dim.x, Thickness), Color);

  // NOTE: Left and right
  PushRect(Group, Offset - V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
  PushRect(Group, Offset + V3(0.5f * Dim.x, 0, 0), V2(Thickness, Dim.y), Color);
}

inline void Clear(render_group *Group, v4 Color) {
  render_entry_clear *Entry = PushRenderElement(Group, render_entry_clear);
  if (Entry) {
    Entry->Color = Color;
  }
}

inline render_entry_coordinate_system *PushCoordninateSystem(render_group *Group, v2 Origin, v2 XAxis, v2 YAxis, v4 Color, loaded_bitmap *Texture, loaded_bitmap *NormalMap, environment_map *Top, environment_map *Middle, environment_map *Bottom) {
  render_entry_coordinate_system *Entry = PushRenderElement(Group, render_entry_coordinate_system);
  if (Entry) {
    Entry->Origin = Origin;
    Entry->XAxis = XAxis;
    Entry->YAxis = YAxis;
    Entry->Color = Color;
    Entry->Texture = Texture;
    Entry->NormalMap = NormalMap;
    Entry->Top = Top;
    Entry->Middle = Middle;
    Entry->Bottom = Bottom;
  }

  return Entry;
}
